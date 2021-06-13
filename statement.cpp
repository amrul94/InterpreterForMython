#include "statement.h"

#include <iostream>
#include <sstream>
#include <utility>

using namespace std;

namespace ast {

    using runtime::Closure;
    using runtime::Context;
    using runtime::ObjectHolder;

    namespace {
        const string ADD_METHOD = "__add__"s;
        const string INIT_METHOD = "__init__"s;
    }  // namespace


    VariableValue::VariableValue(const std::string& var_name)
        : dotted_ids_(std::vector{var_name}) {
    }

    VariableValue::VariableValue(std::vector<std::string> dotted_ids)
        : dotted_ids_(std::move(dotted_ids)) {
    }

    ObjectHolder VariableValue::Execute(Closure& closure, [[maybe_unused]] Context& context) {
        auto found = closure.find(dotted_ids_.front());
        if (found == closure.end()) {
            throw std::runtime_error("Not find variable");
        }
        if (dotted_ids_.size() == 1) {
            return found->second;
        }
        for (size_t i = 1; i < dotted_ids_.size(); ++i) {
            auto instance = found->second.TryAs<runtime::ClassInstance>();
            Closure& fields = instance->Fields();
            found = fields.find(dotted_ids_[i]);
            if (found == fields.end()) {
                throw std::runtime_error("Not find variable");
            }
        }
        return found->second;
    }

    Assignment::Assignment(std::string var, std::unique_ptr<Statement> rv)
        : var_(std::move(var))
        , rv_(std::move(rv)) {
    }

    ObjectHolder Assignment::Execute(Closure& closure, Context& context) {
        auto found = closure.find(var_);
        if (found != closure.end()) {
            return found->second = rv_->Execute(closure, context);
        }
        auto [new_var, _] = closure.emplace(var_, rv_->Execute(closure, context));
        return new_var->second;
    }

    FieldAssignment::FieldAssignment(VariableValue object, std::string field_name,
                                     std::unique_ptr<Statement> rv)
         : object_(std::move(object))
         , field_name_(std::move(field_name))
         , rv_(std::move(rv)) {
    }

    ObjectHolder FieldAssignment::Execute(Closure& closure, Context& context) {
        auto ex = object_.Execute(closure, context);
        auto instance = ex.TryAs<runtime::ClassInstance>();

        Closure& fields = instance->Fields();
        auto found_field = fields.find(field_name_);
        if (found_field != fields.end()) {
            return found_field->second = rv_->Execute(closure, context);
        }
        auto [new_field, _] = fields.emplace(field_name_, rv_->Execute(closure, context));
        return new_field->second;
    }

    Print::Print(unique_ptr<Statement> argument)
        : args_() {
        args_.push_back(std::move(argument));
    }

    Print::Print(vector<unique_ptr<Statement>> args)
        : args_(std::move(args)) {
    }

    unique_ptr<Print> Print::Variable(const std::string& name) {
        auto value = make_unique<VariableValue>(name);
        return make_unique<Print>(std::move(value));
    }

    ObjectHolder Print::Execute(Closure& closure, Context& context) {
        ostream& out = context.GetOutputStream();
        if (args_.empty()) {
            out << '\n';
            return {};
        }
        for (size_t i = 0; i < args_.size() - 1; ++i) {
            auto obj = args_[i]->Execute(closure, context);
            if (obj) {
                obj->Print(out, context);
            } else {
                out << "None"s;
            }
            out << ' ';
        }
        auto obj = args_.back()->Execute(closure, context);
        if (obj) {
            obj->Print(out, context);
        } else {
            out << "None"s;
        }
        out << '\n';
        return {};
    }

    ObjectHolder Stringify::Execute(Closure& closure, Context& context) {
        stringstream strm;
        auto obj = argument_->Execute(closure, context);
        if (obj) {
            obj->Print(strm, context);
        } else {
            strm << "None";
        }
        auto str = strm.str();
        return runtime::ObjectHolder::Own(std::move(runtime::String(strm.str())));
    }

    MethodCall::MethodCall(std::unique_ptr<Statement> object, std::string method,
                           std::vector<std::unique_ptr<Statement>> args)
       : object_(std::move(object))
       , method_(std::move(method))
       , args_(std::move(args)) {
    }

    ObjectHolder MethodCall::Execute(Closure& closure, Context& context) {
        auto obj = object_->Execute(closure, context);
        auto instance = obj.TryAs<runtime::ClassInstance>();

        vector<ObjectHolder> actual_args;
        for (const auto& arg : args_) {
            auto temp_obj = arg->Execute(closure, context);
            actual_args.push_back(temp_obj);
        }
        return instance->Call(method_, actual_args, context);
    }

    ObjectHolder Add::Execute(Closure& closure, Context& context) {
        auto lhs = lhs_->Execute(closure, context);
        auto rhs = rhs_->Execute(closure, context);

        auto lhs_number = lhs.TryAs<runtime::Number>();
        auto rhs_number = rhs.TryAs<runtime::Number>();
        if (lhs_number != nullptr && rhs_number != nullptr) {
            int number = lhs_number->GetValue() + rhs_number->GetValue();
            return runtime::ObjectHolder::Own(runtime::Number{number});
        }

        auto lhs_string = lhs.TryAs<runtime::String>();
        auto rhs_string = rhs.TryAs<runtime::String>();
        if (lhs_string != nullptr && rhs_string != nullptr) {
            string str = lhs_string->GetValue() + rhs_string->GetValue();
            return runtime::ObjectHolder::Own(runtime::String{std::move(str)});
        }

        auto lhs_instance = lhs.TryAs<runtime::ClassInstance>();
        if (lhs_instance != nullptr && lhs_instance->HasMethod(ADD_METHOD, 1)) {
            std::vector<ObjectHolder> actual_args = { rhs };
            return lhs_instance->Call(ADD_METHOD, actual_args, context);
        }

        throw std::runtime_error("No __add__ method"s);
    }

    ObjectHolder Sub::Execute(Closure& closure, Context& context) {
        auto lhs = lhs_->Execute(closure, context);
        auto rhs = rhs_->Execute(closure, context);

        auto lhs_number = lhs.TryAs<runtime::Number>();
        auto rhs_number = rhs.TryAs<runtime::Number>();
        if (lhs_number != nullptr && rhs_number != nullptr) {
            int number = lhs_number->GetValue() - rhs_number->GetValue();
            return runtime::ObjectHolder::Own(runtime::Number{number});
        }

        throw std::runtime_error("lhs or rhs not Number"s);
    }

    ObjectHolder Mult::Execute(Closure& closure, Context& context) {
        auto lhs = lhs_->Execute(closure, context);
        auto rhs = rhs_->Execute(closure, context);

        auto lhs_number = lhs.TryAs<runtime::Number>();
        auto rhs_number = rhs.TryAs<runtime::Number>();
        if (lhs_number != nullptr && rhs_number != nullptr) {
            int number = lhs_number->GetValue() * rhs_number->GetValue();
            return runtime::ObjectHolder::Own(runtime::Number{number});
        }

        throw std::runtime_error("lhs or rhs not Number"s);
    }

    ObjectHolder Div::Execute(Closure& closure, Context& context) {
        auto lhs = lhs_->Execute(closure, context);
        auto rhs = rhs_->Execute(closure, context);

        auto lhs_number = lhs.TryAs<runtime::Number>();
        auto rhs_number = rhs.TryAs<runtime::Number>();
        if (lhs_number != nullptr && rhs_number != nullptr) {
            if (rhs_number->GetValue() == 0) {
                throw std::runtime_error("Division by zero"s);
            }
            int number = lhs_number->GetValue() / rhs_number->GetValue();
            return runtime::ObjectHolder::Own(runtime::Number{number});
        }

        throw std::runtime_error("lhs or rhs not Number"s);
    }

    ObjectHolder Or::Execute(Closure& closure, Context& context) {
        auto lhs = lhs_->Execute(closure, context);
        auto rhs = rhs_->Execute(closure, context);
        bool result = (runtime::IsTrue(lhs) || runtime::IsTrue(rhs));
        return runtime::ObjectHolder::Own(runtime::Bool{result});
    }

    ObjectHolder And::Execute(Closure& closure, Context& context) {
        auto lhs = lhs_->Execute(closure, context);
        auto rhs = rhs_->Execute(closure, context);
        bool result = (runtime::IsTrue(lhs) && runtime::IsTrue(rhs));
        return runtime::ObjectHolder::Own(runtime::Bool{result});
    }

    ObjectHolder Not::Execute(Closure& closure, Context& context) {
        auto arg = argument_->Execute(closure, context);
        bool result = !runtime::IsTrue(arg);
        return runtime::ObjectHolder::Own(runtime::Bool{result});
    }

    ObjectHolder Compound::Execute(Closure& closure, Context& context) {
        for (const auto& stmt : statements_) {
            stmt->Execute(closure, context);
        }
        return {};
    }

    MethodBody::MethodBody(std::unique_ptr<Statement>&& body)
        : body_(std::move(body)) {
    }

    ObjectHolder MethodBody::Execute(Closure& closure, Context& context) {
        try {
            body_->Execute(closure, context);
            return runtime::ObjectHolder::None();
        } catch (runtime::ObjectHolder& obj) {
            return obj;
        }
    }

    ObjectHolder Return::Execute(Closure& closure, Context& context) {
        throw statement_->Execute(closure, context);
    }

    ClassDefinition::ClassDefinition(ObjectHolder cls)
        : cls_(std::move(cls)) {
    }

    ObjectHolder ClassDefinition::Execute(Closure& closure, [[maybe_unused]] Context& context) {
        const auto obj_cls = cls_.TryAs<const runtime::Class>();
        std::string name = obj_cls->GetName();
        const auto [it, _] = closure.emplace(name, cls_);
        return it->second;
    }

    IfElse::IfElse(std::unique_ptr<Statement> condition, std::unique_ptr<Statement> if_body,
                   std::unique_ptr<Statement> else_body)
       : condition_(std::move(condition))
       , if_body_(std::move(if_body))
       , else_body_(std::move(else_body)) {
    }

    ObjectHolder IfElse::Execute(Closure& closure, Context& context) {
        if (runtime::IsTrue(condition_->Execute(closure, context))) {
            return if_body_->Execute(closure, context);
        } else if (else_body_ != nullptr) {
            return else_body_->Execute(closure, context);
        } else {
            return ObjectHolder::None();
        }
    }

    Comparison::Comparison(Comparator cmp, unique_ptr<Statement> lhs, unique_ptr<Statement> rhs)
        : BinaryOperation(std::move(lhs), std::move(rhs))
        , cmp_(std::move(cmp)) {
    }

    ObjectHolder Comparison::Execute(Closure& closure, Context& context) {
        auto lhs = lhs_->Execute(closure, context);
        auto rhs = rhs_->Execute(closure, context);
        bool result = cmp_(lhs, rhs, context);
        return runtime::ObjectHolder::Own(runtime::Bool{result});
    }

    NewInstance::NewInstance(const runtime::Class& cls)
        : instance_(cls) {
    }

    NewInstance::NewInstance(const runtime::Class& cls, std::vector<std::unique_ptr<Statement>> args)
        : instance_(cls)
        , args_(std::move(args)) {
    }


    ObjectHolder NewInstance::Execute(Closure& closure, Context& context) {
        std::vector<ObjectHolder> actual_args;
        for (const auto& arg : args_) {
            actual_args.push_back(arg->Execute(closure, context));

        }
        if (instance_.HasMethod(INIT_METHOD, actual_args.size())) {
            instance_.Call(INIT_METHOD, actual_args, context);
        }
        return ObjectHolder::Share(instance_);
    }

}  // namespace ast