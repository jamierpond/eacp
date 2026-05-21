#include "HooksFormat.h"

#include <Miro/CommandExport/ResolvedTypes.h>

#include <cctype>
#include <optional>
#include <sstream>

namespace eacp::Graphics::Codegen
{

namespace
{

using Miro::CommandExport::CommandEntry;
using Miro::CommandExport::ResolvedTypes;
using Miro::CommandExport::resolveTypes;
using Miro::TypeTree::PrimitiveKind;
using Miro::TypeTree::TypeNode;

std::string_view tsPrimitiveLocal(PrimitiveKind kind)
{
    switch (kind)
    {
        case PrimitiveKind::Boolean:
            return "boolean";
        case PrimitiveKind::String:
            return "string";
        case PrimitiveKind::Number:
        case PrimitiveKind::Integer:
        case PrimitiveKind::Int64:
            return "number";
    }
    return "unknown";
}

std::string capitalizeFirst(std::string_view s)
{
    if (s.empty())
        return {};

    auto r = std::string {s};
    r[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(r[0])));
    return r;
}

std::string stripTrailing(std::string s, std::string_view suffix)
{
    if (s.size() >= suffix.size()
        && std::string_view {s}.substr(s.size() - suffix.size()) == suffix)
        s.resize(s.size() - suffix.size());

    return s;
}

const TypeNode* findRootByQualified(std::span<const TypeNode> roots,
                                    std::string_view qualified)
{
    for (auto& r: roots)
        if (r.qualifiedName == qualified)
            return &r;

    return nullptr;
}

const TypeNode::Field* findField(const TypeNode& node, std::string_view name)
{
    for (auto& f: node.fields)
        if (f.name == name)
            return &f;

    return nullptr;
}

struct KeyedInfo
{
    const TypeNode* itemType = nullptr;
    std::string_view keyTsType;
};

std::optional<KeyedInfo> resolveKeyedInfo(const TypeNode& payload,
                                          const EventEntry& event)
{
    if (!event.isKeyed)
        return std::nullopt;

    auto* collField = findField(payload, event.collectionField);
    if (collField == nullptr || collField->type == nullptr)
        return std::nullopt;

    auto& collNode = *collField->type;
    if (collNode.shape != TypeNode::Shape::Array || collNode.inner == nullptr)
        return std::nullopt;

    auto& itemNode = *collNode.inner;
    if (itemNode.shape != TypeNode::Shape::Object)
        return std::nullopt;

    auto* keyField = findField(itemNode, event.keyField);
    if (keyField == nullptr || keyField->type == nullptr)
        return std::nullopt;

    auto& keyNode = *keyField->type;
    if (keyNode.shape != TypeNode::Shape::Primitive)
        return std::nullopt;

    return KeyedInfo {&itemNode, tsPrimitiveLocal(keyNode.primitive)};
}

bool commandExists(std::span<const CommandEntry> commands,
                   std::string_view name)
{
    for (auto& c: commands)
        if (c.name == name)
            return true;

    return false;
}

std::string initialJsonFor(const EventEntry& event)
{
    if (!event.defaultPayloadJson)
        return "{}";

    return Miro::Json::print(event.defaultPayloadJson());
}

} // namespace

std::string formatHooksModule(std::span<TypeNode> typeRoots,
                              std::span<const CommandEntry> commands,
                              std::span<const EventEntry> events,
                              std::string_view /*baseName*/)
{
    auto resolved = resolveTypes(typeRoots);

    auto body = std::ostringstream {};
    auto usedKeyed = false;
    auto usedBridge = false;
    auto usedNative = false;

    for (auto& event: events)
    {
        auto* payloadNode = findRootByQualified(typeRoots, event.payloadQualifiedName);
        if (payloadNode == nullptr)
            continue;

        auto getCmdName = "get" + capitalizeFirst(event.name);
        auto hasGetCmd = commandExists(commands, getCmdName);
        auto stateHookName = "use" + capitalizeFirst(event.name);
        auto initialJson = initialJsonFor(event);

        if (event.isKeyed && hasGetCmd)
        {
            auto keyed = resolveKeyedInfo(*payloadNode, event);
            if (!keyed)
                continue;

            auto itemTypeName =
                resolved.nameFor(keyed->itemType->qualifiedName,
                                 keyed->itemType->typeName);
            auto idsBase = stripTrailing(itemTypeName, "Item");
            auto storeVar = event.name + "Store";

            body << "\n"
                 << "const " << storeVar << " = makeKeyedStore({\n"
                 << "    backend,\n"
                 << "    event: '" << event.name << "',\n"
                 << "    fetch: backend." << getCmdName << ",\n"
                 << "    initial: " << initialJson << ",\n"
                 << "    getItems: (s) => s." << event.collectionField << ",\n"
                 << "    getKey:   (i) => i." << event.keyField << ",\n"
                 << "});\n"
                 << "export const " << stateHookName << " = " << storeVar
                 << ".useAll;\n"
                 << "export const use" << idsBase << "Ids = " << storeVar
                 << ".useIds;\n"
                 << "export const use" << itemTypeName << " = " << storeVar
                 << ".useItem;\n";

            usedKeyed = true;
            continue;
        }

        if (hasGetCmd)
        {
            body << "\n"
                 << "export const " << stateHookName << " = makeBridgeStore({\n"
                 << "    backend,\n"
                 << "    event: '" << event.name << "',\n"
                 << "    fetch: backend." << getCmdName << ",\n"
                 << "    initial: " << initialJson << ",\n"
                 << "});\n";

            usedBridge = true;
            continue;
        }

        body << "\n"
             << "export const " << stateHookName << " = makeNativeEvent({\n"
             << "    backend,\n"
             << "    event: '" << event.name << "',\n"
             << "    initial: " << initialJson << ",\n"
             << "});\n";

        usedNative = true;
    }

    auto out = std::ostringstream {};
    out << "// Generated. Do not edit by hand.\n"
        << "//\n"
        << "// Pre-wired React hooks for every registered bridge event.\n"
        << "// Keyed states get useXxx / useXxxIds / useXxxItem; plain\n"
        << "// states get useXxx; push-only events get useXxx via\n"
        << "// makeNativeEvent. Initial values come from toJSON(T{}).\n";

    if (!usedKeyed && !usedBridge && !usedNative)
    {
        out << "\n// (No state-backed events registered.)\n";
        return out.str();
    }

    out << "\nimport { backend } from './backend';\n";

    auto helpers = EA::Vector<std::string> {};
    if (usedBridge)
        helpers.add("makeBridgeStore");
    if (usedKeyed)
        helpers.add("makeKeyedStore");
    if (usedNative)
        helpers.add("makeNativeEvent");

    out << "import { ";
    for (auto i = 0; i < helpers.size(); ++i)
        out << (i == 0 ? "" : ", ") << helpers[i];
    out << " } from './react';\n";

    out << body.str();

    return out.str();
}

} // namespace eacp::Graphics::Codegen
