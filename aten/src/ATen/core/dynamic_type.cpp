#include <ATen/core/dynamic_type.h>

#include <string>

#include <ATen/core/function_schema.h>
#include <ATen/core/ivalue.h>
#include <ATen/core/jit_type.h>
#include <c10/util/Exception.h>

namespace c10 {

namespace {
bool contains(DynamicType::Tag lhs, DynamicTypeBits rhs) {
  return (static_cast<DynamicTypeBits>(lhs) | rhs) ==
      static_cast<DynamicTypeBits>(lhs);
}
bool contains(DynamicType::Tag lhs, DynamicType::Tag rhs) {
  return contains(lhs, static_cast<DynamicTypeBits>(rhs));
}
} // namespace

std::string DynamicType::str() const {
  std::string ret = "Dynamic<";
  ret += std::to_string(static_cast<DynamicTypeBits>(tag_));
  ret += ">";
  if (tag_ == Tag::Class) {
    auto name = class_->name();
    ret += "[" + (name ? name->qualifiedName() : "Unknown Class") + "]";
  } else if (tag_ == Tag::Singleton) {
    ret += "[kind:" + std::to_string(static_cast<int>(typeKind_)) + "]";
  } else if (arguments_.elems.size() > 0) {
    ret += "[";
    for (const auto& arg : arguments_.elems) {
      if (arg.label) {
        ret += *arg.label + ":";
      }
      ret += arg.ty->str();
      ret += ",";
    }
    ret += "]";
  }
  return ret;
}

DynamicType::Arguments::Arguments(c10::ArrayRef<TypePtr> args) {
  elems.reserve(args.size());
  for (const auto& arg : args) {
    elems.emplace_back(create(*arg));
  }
}

DynamicType::Arguments::Arguments(
    const std::vector<c10::string_view>& names,
    c10::ArrayRef<TypePtr> args)
    : Arguments(args) {
  TORCH_INTERNAL_ASSERT(names.size() == args.size());
  for (size_t i = 0; i < args.size(); i++) {
    elems[i].label = std::string{names[i]};
  }
}

DynamicType::~DynamicType() {
  if (tag_ == Tag::Singleton) {
    return;
  }

  if (tag_ == Tag::Class) {
    class_.~ClassTypePtr();
    return;
  }

  arguments_.~Arguments();
}

std::shared_ptr<const DynamicType> DynamicType::create(const Type& other) {
  if (auto dyn = other.cast<DynamicType>()) {
    return dyn;
  }
  return std::shared_ptr<const DynamicType>(new DynamicType{other});
}

DynamicTypePtr DynamicType::create(Type& other) {
  if (auto dyn = other.cast<DynamicType>()) {
    return dyn;
  }
  return std::shared_ptr<DynamicType>(new DynamicType{other});
}

DynamicType::DynamicType(const Type& other) : Type(DynamicType::Kind) {
  auto kind = other.kind();
  TORCH_INTERNAL_ASSERT(kind != Kind);
  if (auto cls = other.cast<ClassType>()) {
    new (&class_) ClassTypePtr(std::move(cls));
    tag_ = Tag::Class;
    return;
  }
  switch (kind) {
#define CASE_TYPE(T, _) \
  case T##Type::Kind:   \
    tag_ = Tag::T;      \
    break;
    FORALL_DYNAMIC_TYPES(CASE_TYPE)
#undef CASE_TYPE
    default:
      if (kind == DeviceObjType::Kind || kind == StreamObjType::Kind ||
          kind == CapsuleType::Kind) {
        tag_ = Tag::Singleton;
        typeKind_ = kind;
        return;
      }
      TORCH_INTERNAL_ASSERT(false, "Unsupported dynamic type: ", other.str());
  }

  auto args = other.containedTypes();
  if (args.empty()) {
    new (&arguments_) Arguments();
    return;
  }

  if (auto tup = other.castRaw<TupleType>()) {
    if (auto schema = tup->schema()) {
      std::vector<c10::string_view> names;
      for (const auto& args : schema->arguments()) {
        names.push_back(args.name());
      }
      new (&arguments_) Arguments(names, args);
      return;
    }
  }

  new (&arguments_) Arguments(args);
}

bool DynamicType::equals(const DynamicType& other) const {
  if (this == &other) {
    return true;
  }
  if (tag_ != other.tag_) {
    return false;
  }
  switch (tag_) {
    case Tag::Class:
      return *class_ == *other.class_;
    case Tag::Singleton:
      return typeKind_ == other.typeKind_;
    default:
      return compareArguments(
          other, [](const LabeledDynamicType& a, const LabeledDynamicType& b) {
            return a.equals(b);
          });
  }
}

bool DynamicType::operator==(const Type& rhs) const {
  return equals(*create(rhs));
}

bool DynamicType::isSubtypeOfExt(const Type& rhs, std::ostream*) const {
  auto other = create(rhs);
  if (tag_ == other->tag_) {
    if (equals(*other)) {
      return true;
    }
    if (contains(tag_, kDynamicCovariantTypeBit)) {
      if (compareArguments(
              *other,
              [](const LabeledDynamicType& a, const LabeledDynamicType& b) {
                return a.isSubtypeOf(b);
              })) {
        return true;
      };
    }
  } else if (contains(other->tag_, tag_)) {
    return true;
  }

  if (other->tag_ == Tag::Optional) {
    if (isSubtypeOf(other->arguments_.elems[0].ty)) {
      return true;
    }
  }

  return false;
}

bool LabeledDynamicType::isSubtypeOf(const LabeledDynamicType& other) const {
  if (!other.label || (label == other.label)) {
    return ty->isSubtypeOf(other.ty);
  }

  return false;
}

bool LabeledDynamicType::equals(const LabeledDynamicType& other) const {
  return (label == other.label) && (*ty == *other.ty);
}

} // namespace c10
