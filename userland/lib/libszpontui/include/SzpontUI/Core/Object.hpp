#pragma once

#include <string>
#include <vector>
#include <memory>
#include <algorithm>

namespace SzpontUI {

class Object : public std::enable_shared_from_this<Object> {
public:
    explicit Object(std::string name = "") : name_(std::move(name)) {}
    virtual ~Object() {
        for (auto &child : children_) {
            child->parent_ = nullptr;
        }
        children_.clear();
    }

    const std::string &name() const { return name_; }
    void set_name(std::string name) { name_ = std::move(name); }

    Object *parent() const { return parent_; }

    const std::vector<std::shared_ptr<Object>> &children() const {
        return children_;
    }

    virtual void add_child(std::shared_ptr<Object> child) {
        if (!child || child.get() == this) return;
        if (child->parent_) {
            child->parent_->remove_child(child);
        }
        child->parent_ = this;
        children_.push_back(child);
    }

    virtual void remove_child(std::shared_ptr<Object> child) {
        if (!child) return;
        auto it = std::find(children_.begin(), children_.end(), child);
        if (it != children_.end()) {
            (*it)->parent_ = nullptr;
            children_.erase(it);
        }
    }

private:
    std::string name_;
    Object *parent_{nullptr};
    std::vector<std::shared_ptr<Object>> children_;
};

} // namespace SzpontUI
