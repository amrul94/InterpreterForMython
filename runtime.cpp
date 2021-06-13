#include "runtime.h"

#include <algorithm>
#include <cassert>
#include <optional>
#include <sstream>
#include <utility>

using namespace std;

namespace runtime {

    ObjectHolder::ObjectHolder(std::shared_ptr<Object> data)
            : data_(std::move(data)) {
    }

    void ObjectHolder::AssertIsValid() const {
        assert(data_ != nullptr);
    }

    ObjectHolder ObjectHolder::Share(Object& object) {
        // Возвращаем не владеющий shared_ptr (его deleter ничего не делает)
        return ObjectHolder(std::shared_ptr<Object>(&object, [](auto* /*p*/) { /* do nothing */ }));
    }

    ObjectHolder ObjectHolder::None() {
        return ObjectHolder();
    }

    Object& ObjectHolder::operator*() const {
        AssertIsValid();
        return *Get();
    }

    Object* ObjectHolder::operator->() const {
        AssertIsValid();
        return Get();
    }

    Object* ObjectHolder::Get() const {
        return data_.get();
    }

    ObjectHolder::operator bool() const {
        return Get() != nullptr;
    }

    bool IsTrue(const ObjectHolder& object) {
        if (!object) {
            return false;
        }
        auto boolean = object.TryAs<Bool>();
        if (boolean != nullptr) {
            return boolean->GetValue();
        }

        auto number = object.TryAs<Number>();
        if (number != nullptr) {
            return number->GetValue();
        }

        auto str = object.TryAs<String>();
        if (str != nullptr) {
            return !str->GetValue().empty();
        }

        return false;
    }

    ClassInstance::ClassInstance(const Class& cls)
            : cls_(cls) {
    }

    void ClassInstance::Print(std::ostream& os, Context& context) {
        std::vector<ObjectHolder> actual_args;
        if (HasMethod("__str__"s, 0)) {
            Call("__str__"s, actual_args, context)->Print(os, context);
        } else {
            os << this;
        }
    }

    bool ClassInstance::HasMethod(const std::string& method, size_t argument_count) const {
        auto found_method = cls_.GetMethod(method);
        if (found_method == nullptr) {
            return false;
        }
        return found_method->formal_params.size() == argument_count;
    }

    Closure& ClassInstance::Fields() {
        return fields_;
    }

    const Closure& ClassInstance::Fields() const {
        return fields_;
    }

    ObjectHolder ClassInstance::Call(const std::string& method,
                                     const std::vector<ObjectHolder>& actual_args,
                                     Context& context) {
        if (!HasMethod(method, actual_args.size())) {
            throw runtime_error("There is no method " + method + "in the class " + cls_.GetName());
        }
        auto found_method = cls_.GetMethod(method);
        Closure closure;
        closure.emplace("self", ObjectHolder::Share(*this));
        for (size_t i = 0; i < found_method->formal_params.size(); ++i) {
            closure.emplace(found_method->formal_params[i], actual_args[i]);
        }
        return found_method->body->Execute(closure, context);
    }

    Class::Class(std::string name, std::vector<Method> methods, const Class* parent)
            : name_(std::move(name))
            , methods_(std::move(methods))
            , parent_(parent) {
        auto lambda = [](const Method& lhs, const Method& rhs) {
            return lhs.name < rhs.name;
        };
        std::sort(methods.begin(), methods.end(), lambda);
    }

    const Method* Class::GetMethod(const std::string& name) const {
        auto lambda = [name](const Method& lhs) {
            return lhs.name == name;
        };

        auto found_in_class = std::find_if(methods_.begin(), methods_.end(), lambda);
        if (found_in_class != methods_.end()) {
            return &(*found_in_class);
        }

        if (parent_ == nullptr) {
            return nullptr;
        }

        auto found_in_parents = std::find_if(parent_->methods_.begin(), parent_->methods_.end(), lambda);
        if (found_in_parents != parent_->methods_.end()) {
            return &(*found_in_parents);
        }
        return nullptr;
    }

    [[nodiscard]] const std::string& Class::GetName() const {
        return name_;
    }

    void Class::Print(ostream& os, [[maybe_unused]] Context& context) {
        os << "Class " << name_;
    }

    void Bool::Print(std::ostream& os, [[maybe_unused]] Context& context) {
        os << (GetValue() ? "True"sv : "False"sv);
    }

    namespace {
        template <typename T>
        bool TypeChecking(const ObjectHolder& lhs, const ObjectHolder& rhs) {
            auto lhs_t = lhs.TryAs<T>();
            auto rhs_t = rhs.TryAs<T>();
            return lhs_t != nullptr && rhs_t != nullptr;
        }

        template <typename T>
        bool EqualImpl(const ObjectHolder& lhs, const ObjectHolder& rhs) {
            return lhs.TryAs<T>()->GetValue() == rhs.TryAs<T>()->GetValue();
        }

        template <typename T>
        bool LessImpl(const ObjectHolder& lhs, const ObjectHolder& rhs) {
            return lhs.TryAs<T>()->GetValue() < rhs.TryAs<T>()->GetValue();
        }

    }

    bool Equal(const ObjectHolder& lhs, const ObjectHolder& rhs, Context& context) {
        if (!lhs && !rhs) {
            return true;
        }

        if (TypeChecking<Bool>(lhs, rhs)) {
            return EqualImpl<Bool>(lhs, rhs);
        }

        if (TypeChecking<Number>(lhs, rhs)) {
            return EqualImpl<Number>(lhs, rhs);
        }

        if (TypeChecking<String>(lhs, rhs)) {
            return EqualImpl<String>(lhs, rhs);
        }

        auto lhs_instance = lhs.TryAs<ClassInstance>();
        if (lhs_instance != nullptr) {
            const auto result = lhs_instance->Call("__eq__"s, {rhs}, context);
            return result.TryAs<Bool>()->GetValue();
        }

        throw std::runtime_error("Cannot compare objects for equality"s);
    }

    bool Less(const ObjectHolder& lhs, const ObjectHolder& rhs, Context& context) {
        if (!lhs || !rhs) {
            throw std::runtime_error("Cannot compare objects for less"s);
        }

        if (TypeChecking<Bool>(lhs, rhs)) {
            return LessImpl<Bool>(lhs, rhs);
        }

        if (TypeChecking<Number>(lhs, rhs)) {
            return LessImpl<Number>(lhs, rhs);
        }

        if (TypeChecking<String>(lhs, rhs)) {
            return LessImpl<String>(lhs, rhs);
        }

        auto lhs_instance = lhs.TryAs<ClassInstance>();
        if (lhs_instance != nullptr) {
            const auto result = lhs_instance->Call("__lt__"s, {rhs}, context);
            return static_cast<bool>(result.TryAs<Bool>()->GetValue());
        }

        throw std::runtime_error("Cannot compare objects for less"s);
    }

    bool NotEqual(const ObjectHolder& lhs, const ObjectHolder& rhs, Context& context) {
        return !Equal(lhs, rhs, context);
    }

    bool Greater(const ObjectHolder& lhs, const ObjectHolder& rhs, Context& context) {
        return !Less(lhs, rhs, context) && !Equal(lhs, rhs, context);
    }

    bool LessOrEqual(const ObjectHolder& lhs, const ObjectHolder& rhs, Context& context) {
        return Less(lhs, rhs, context) || Equal(lhs, rhs, context);
    }

    bool GreaterOrEqual(const ObjectHolder& lhs, const ObjectHolder& rhs, Context& context) {
        return !Less(lhs, rhs, context);
    }

}  // namespace runtime