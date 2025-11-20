#include "AdjustableComponent.h"
#include "ComponentLibrary.h"
#include "../GlobalValueManager.h"
#include "../Object.h"
#include <iostream>

AdjustableComponent::AdjustableComponent(Object& parent)
    : Component(parent)
    , globalValueName("")
    , operation("set")
    , value(0.0f)
    , createIfNotExists(true) {
}

AdjustableComponent::AdjustableComponent(Object& parent, const nlohmann::json& data)
    : Component(parent)
    , globalValueName(data.value("globalValueName", std::string("")))
    , operation(data.value("operation", std::string("set")))
    , value(data.value("value", 0.0f))
    , createIfNotExists(data.value("createIfNotExists", true)) {
}

void AdjustableComponent::update(float deltaTime) {
    // No per-frame update needed
    (void)deltaTime;
}

void AdjustableComponent::draw() {
    // No drawing needed
}

void AdjustableComponent::use(Object& instigator) {
    if (globalValueName.empty()) {
        std::cerr << "[AdjustableComponent] No global value name specified" << std::endl;
        return;
    }

    GlobalValueManager& gvm = GlobalValueManager::getInstance();
    
    // Check if value exists
    if (!gvm.hasValue(globalValueName)) {
        if (!createIfNotExists) {
            std::cerr << "[AdjustableComponent] Global value '" << globalValueName << "' does not exist and createIfNotExists is false" << std::endl;
            return;
        }
        // Create with initial value of 0
        gvm.setValue(globalValueName, 0.0f);
    }

    // Apply operation
    if (operation == "set") {
        gvm.setValue(globalValueName, value);
    } else if (operation == "add") {
        gvm.modifyValue(globalValueName, value);
    } else if (operation == "subtract") {
        gvm.modifyValue(globalValueName, -value);
    } else if (operation == "multiply") {
        float current = gvm.getValue(globalValueName);
        gvm.setValue(globalValueName, current * value);
    } else if (operation == "divide") {
        if (std::abs(value) < 0.0001f) {
            std::cerr << "[AdjustableComponent] Division by zero or near-zero value" << std::endl;
            return;
        }
        float current = gvm.getValue(globalValueName);
        gvm.setValue(globalValueName, current / value);
    } else {
        std::cerr << "[AdjustableComponent] Unknown operation: " << operation << std::endl;
        return;
    }

    std::cout << "[AdjustableComponent] Applied " << operation << " operation to '" << globalValueName 
              << "' with value " << value << std::endl;
}

nlohmann::json AdjustableComponent::toJson() const {
    nlohmann::json data;
    data["type"] = getTypeName();
    data["globalValueName"] = globalValueName;
    data["operation"] = operation;
    data["value"] = value;
    data["createIfNotExists"] = createIfNotExists;
    return data;
}

// Register this component type with the library
static ComponentRegistrar<AdjustableComponent> registrar("AdjustableComponent");

