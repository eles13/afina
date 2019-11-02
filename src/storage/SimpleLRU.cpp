#include "SimpleLRU.h"

namespace Afina {
namespace Backend {

// See MapBasedGlobalLockImpl.h
bool SimpleLRU::Put(const std::string &key, const std::string &value) {
    if ((key.size() + value.size()) > _max_size)
        return false;
    auto it = _lru_index.find(key);
    if (it != _lru_index.end()) {
        it->second.get().value = value;
        while (_cursize + key.size() + value.size() > _max_size)
            if (!Delete(_lru_head->key))
                return false;
        _cursize += key.size() + value.size();
        return to_tail(it->second.get());
    }
    return push(key, value);
}

// See MapBasedGlobalLockImpl.h
bool SimpleLRU::PutIfAbsent(const std::string &key, const std::string &value) {
    if ((key.size() + value.size()) > _max_size)
        return false;
    if (_lru_index.find((key)) == _lru_index.end())
        return push(key, value);
    return false;
}

// See MapBasedGlobalLockImpl.h
bool SimpleLRU::Set(const std::string &key, const std::string &value) {
    if (key.size() + value.size() > _max_size)
        return false;
    auto it = _lru_index.find((key));
    if (it == _lru_index.end())
        return false;
    it->second.get().value = value;
    while (_cursize + key.size() + value.size() > _max_size)
        if (!Delete(_lru_head->key))
            return false;
    _cursize += key.size() + value.size();
    return to_tail(it->second.get());
}

// See MapBasedGlobalLockImpl.h
bool SimpleLRU::Delete(const std::string &key) {
    auto it = _lru_index.find(key);
    if (it == _lru_index.end())
        return false;
    return remove(it->second.get());
}

// See MapBasedGlobalLockImpl.h
bool SimpleLRU::Get(const std::string &key, std::string &value) {
    auto it = _lru_index.find((key));
    if (it == _lru_index.end())
        return false;
    value = it->second.get().value;
    return to_tail(it->second.get());
}

bool SimpleLRU::remove(lru_node &node) {
    _cursize -= node.key.size() + node.value.size();
    _lru_index.erase(node.key);
    std::unique_ptr<lru_node> tmp = nullptr;
    if (_lru_tail->prev == nullptr) {
        _lru_head.swap(tmp);
        _lru_tail = nullptr;
        return true;
    }
    if (&node == _lru_head.get()) {
        tmp.swap(_lru_head);
        _lru_head.swap(tmp->next);
        _lru_head->prev = nullptr;
    } else if (&node == _lru_tail) {
        tmp.swap(_lru_tail->prev->next);
        _lru_tail = tmp->prev;
        _lru_tail->next.reset(nullptr);
    } else {
        tmp.swap(node.prev->next);
        tmp->next->prev = tmp->prev;
        tmp->prev->next.swap(tmp->next);
    }
    return true;
}

bool SimpleLRU::push(const std::string &key, const std::string &value) {
    while (_cursize + key.size() + value.size() > _max_size)
        if (!Delete(_lru_head->key))
            return false;
    _cursize += key.size() + value.size();
    auto new_node = new lru_node(key, value);
    if (_lru_tail != nullptr) {
        new_node->prev = _lru_tail;
        _lru_tail->next.reset(new_node);
        _lru_tail = _lru_tail->next.get();
    } else {
        _lru_tail = new_node;
        _lru_head.reset(new_node);
    }
    _lru_index.insert(std::make_pair(std::reference_wrapper<const std::string>(_lru_tail->key),
                                     std::reference_wrapper<lru_node>(*_lru_tail)));
    return true;
}
bool SimpleLRU::to_tail(lru_node &node) {
    if (&node == _lru_tail)
        return true;
    if (node.prev == nullptr) {
        _lru_head.swap(node.next);
        _lru_head->prev = nullptr;
    } else {
        node.next->prev = node.prev;
        node.prev->next.swap(node.next);
    }
    _lru_tail->next.swap(node.next);
    node.prev = _lru_tail;
    _lru_tail = &node;
    return true;
}
} // namespace Backend
} // namespace Afina
