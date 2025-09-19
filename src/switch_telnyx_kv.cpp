#include "switch_telnyx_kv.h"
#include <vector>
#include <string>
#include <unordered_map>

class LockGuard {
public:
    explicit LockGuard(switch_thread_rwlock_t* lock, bool write_lock = false) 
        : _rwlock(lock), is_write_lock(write_lock) {
        if (is_write_lock) {
            switch_thread_rwlock_wrlock(_rwlock);
        } else {
            switch_thread_rwlock_rdlock(_rwlock);
        }
    }
    
    ~LockGuard() {
        switch_thread_rwlock_unlock(_rwlock);
    }
    
    LockGuard(const LockGuard&) = delete;
    LockGuard& operator=(const LockGuard&) = delete;

private:
    switch_thread_rwlock_t* _rwlock;
    bool is_write_lock;

}; // class LockGuard

class KVManager {
public:
	struct ModuleCallbacks {
		std::string name;
		switch_telnyx_kv_callbacks_t* callbacks;
		void* user_data;
	};
	
	typedef std::vector<ModuleCallbacks> KVModules;
	typedef std::unordered_map<std::string, size_t> KVModuleIndex;

private:
    KVManager() : _rwlock(nullptr) {}
    
public:
    static KVManager& instance() {
        if (!_instance) {
            _instance = new KVManager();
        }
        return *_instance;
    }

	static void destroy() {
		if (_instance) {
			delete _instance;
			_instance = nullptr;
		}
	}
    
    KVManager(const KVManager&) = delete;
    KVManager& operator=(const KVManager&) = delete;

public:
    void init(switch_memory_pool_t *pool) {
        switch_thread_rwlock_create(&_rwlock, pool);
    }
    
    void deinit() {
        if (_rwlock) {
            switch_thread_rwlock_wrlock(_rwlock);
            _kv_modules.clear();
            _kv_module_index.clear();
            switch_thread_rwlock_unlock(_rwlock);
            
			// Explicitly destroy the lock
			switch_thread_rwlock_destroy(_rwlock);
            _rwlock = nullptr;
        }
    }
    
    switch_thread_rwlock_t* getLock() { return _rwlock; }
    KVModules& modules() { return _kv_modules; }
    KVModuleIndex& index() { return _kv_module_index; }
        
    switch_status_t getModule(const char* module_name, const char* key, const char* ns, char** value) {
        LockGuard guard(_rwlock);
        
        if (!zstr(module_name)) {
            // Use specific module - fast hash lookup
            auto* module = findModule(module_name);
            if (module) {
                if (module->callbacks && module->callbacks->get) {
                    return module->callbacks->get(key, ns, value, module->user_data);
                } else {
                    return SWITCH_STATUS_NOTIMPL;
                }
            }
            return SWITCH_STATUS_NOTFOUND;
        } else {
            // Use first available module - iterate in registration order
            for (auto& module : _kv_modules) {
                if (module.callbacks) {
                    if (module.callbacks->get) {
                        return module.callbacks->get(key, ns, value, module.user_data);
                    } else {
                        return SWITCH_STATUS_NOTIMPL;
                    }
                }
            }
        }
        return SWITCH_STATUS_FALSE;
    }
    
    switch_status_t setKV(const char* module_name, const char* key, const char* value, const char* ns) {
        LockGuard guard(_rwlock);
        
        if (!zstr(module_name)) {
            // Use specific module - fast hash lookup
            auto* module = findModule(module_name);
            if (module) {
                if (module->callbacks && module->callbacks->set) {
                    return module->callbacks->set(key, value, ns, module->user_data);
                } else {
                    return SWITCH_STATUS_NOTIMPL;
                }
            }
            return SWITCH_STATUS_NOTFOUND;
        } else {
            // Use first available module - iterate in registration order
            for (auto& module : _kv_modules) {
                if (module.callbacks) {
                    if (module.callbacks->set) {
                        return module.callbacks->set(key, value, ns, module.user_data);
                    } else {
                        return SWITCH_STATUS_NOTIMPL;
                    }
                }
            }
        }
        return SWITCH_STATUS_NOTFOUND;
    }
    
    switch_status_t setTTLKV(const char* module_name, const char* key, const char* value, const char* ns, uint32_t ttl_seconds) {
        LockGuard guard(_rwlock);
        
        if (!zstr(module_name)) {
            // Use specific module - fast hash lookup
            auto* module = findModule(module_name);
            if (module) {
                if (module->callbacks && module->callbacks->set_ttl) {
                    return module->callbacks->set_ttl(key, value, ns, ttl_seconds, module->user_data);
                } else {
                    return SWITCH_STATUS_NOTIMPL;
                }
            }
            return SWITCH_STATUS_NOTFOUND;
        } else {
            // Use first available module - iterate in registration order
            for (auto& module : _kv_modules) {
                if (module.callbacks) {
                    if (module.callbacks->set_ttl) {
                        return module.callbacks->set_ttl(key, value, ns, ttl_seconds, module.user_data);
                    } else {
                        return SWITCH_STATUS_NOTIMPL;
                    }
                }
            }
        }
        return SWITCH_STATUS_NOTFOUND;
    }
    
    switch_status_t deleteKV(const char* module_name, const char* key, const char* ns) {
        LockGuard guard(_rwlock);
        
        if (!zstr(module_name)) {
            // Use specific module - fast hash lookup
            auto* module = findModule(module_name);
            if (module) {
                if (module->callbacks && module->callbacks->del) {
                    return module->callbacks->del(key, ns, module->user_data);
                } else {
                    return SWITCH_STATUS_NOTIMPL;
                }
            }
            return SWITCH_STATUS_NOTFOUND;
        } else {
            // Use first available module - iterate in registration order
            for (auto& module : _kv_modules) {
                if (module.callbacks) {
                    if (module.callbacks->del) {
                        return module.callbacks->del(key, ns, module.user_data);
                    } else {
                        return SWITCH_STATUS_NOTIMPL;
                    }
                }
            }
        }
        return SWITCH_STATUS_NOTFOUND;
    }
    
    void registerModule(const char* module_name, switch_telnyx_kv_callbacks_t* callbacks, void* user_data) {
        if (!module_name || !callbacks) {
            return;
        }
        
        LockGuard guard(_rwlock, true);
        
        // Remove existing module if it exists
        removeModule(module_name);
        
        // Add new module at the end (maintains registration order)
        size_t new_index = _kv_modules.size();
        _kv_modules.push_back({module_name, callbacks, user_data});
        _kv_module_index[module_name] = new_index;
    }
    
    void unregisterModule(const char* module_name) {
        if (zstr(module_name)) {
            return;
        }
        
        LockGuard guard(_rwlock, true);
        removeModule(module_name);
    }
    
    switch_bool_t moduleExists(const char* module_name) {
        if (zstr(module_name)) {
            return SWITCH_FALSE;
        }
        
        LockGuard guard(_rwlock);
        return (_kv_module_index.find(module_name) != _kv_module_index.end()) ? SWITCH_TRUE : SWITCH_FALSE;
    }

private:
    ModuleCallbacks* findModule(const std::string& name) {
        auto it = _kv_module_index.find(name);
        if (it != _kv_module_index.end() && it->second < _kv_modules.size()) {
            return &_kv_modules[it->second];
        }
        return nullptr;
    }

    void removeModule(const std::string& module_name) {
        auto index_it = _kv_module_index.find(module_name);
        if (index_it != _kv_module_index.end()) {
            size_t index = index_it->second;
            if (index < _kv_modules.size()) {
                _kv_modules.erase(_kv_modules.begin() + index);
                // Update indices for all modules that shifted down
                for (auto& [name, idx] : _kv_module_index) {
                    if (idx > index) {
                        idx--;
                    }
                }
            }
            _kv_module_index.erase(index_it);
        }
    }


private:
	KVModules _kv_modules;
	KVModuleIndex _kv_module_index;
    switch_thread_rwlock_t* _rwlock;

	static KVManager* _instance;
}; // class KVManager

KVManager* KVManager::_instance = nullptr;

void switch_telnyx_kv_init(switch_memory_pool_t *pool)
{
	KVManager::instance().init(pool);
}

void switch_telnyx_kv_deinit()
{
	KVManager::instance().deinit();
	KVManager::destroy();
}

/////////////////////////////////////////////////////////////////////
// C API KV functions

switch_status_t switch_telnyx_kv_get_module(const char* module_name, const char* key, const char* ns, char** value)
{
	return KVManager::instance().getModule(module_name, key, ns, value);
}

switch_status_t switch_telnyx_kv_get(const char* key, const char* ns, char** value)
{
	return switch_telnyx_kv_get_module(nullptr, key, ns, value);
}

switch_status_t switch_telnyx_kv_set_module(const char* module_name, const char* key, const char* value, const char* ns)
{
	return KVManager::instance().setKV(module_name, key, value, ns);
}

switch_status_t switch_telnyx_kv_set(const char* key, const char* value, const char* ns)
{
	return switch_telnyx_kv_set_module(nullptr, key, value, ns);
}

switch_status_t switch_telnyx_kv_set_ttl_module(const char* module_name, const char* key, const char* value, const char* ns, uint32_t ttl_seconds)
{
	return KVManager::instance().setTTLKV(module_name, key, value, ns, ttl_seconds);
}

switch_status_t switch_telnyx_kv_set_ttl(const char* key, const char* value, const char* ns, uint32_t ttl_seconds)
{
	return switch_telnyx_kv_set_ttl_module(nullptr, key, value, ns, ttl_seconds);
}

switch_status_t switch_telnyx_kv_delete_module(const char* module_name, const char* key, const char* ns)
{
	return KVManager::instance().deleteKV(module_name, key, ns);
}

switch_status_t switch_telnyx_kv_delete(const char* key, const char* ns)
{
	return switch_telnyx_kv_delete_module(nullptr, key, ns);
}

void switch_telnyx_kv_register_callbacks(const char* module_name, switch_telnyx_kv_callbacks_t* callbacks, void* user_data)
{
	KVManager::instance().registerModule(module_name, callbacks, user_data);
}

void switch_telnyx_kv_unregister_callbacks(const char* module_name)
{
	KVManager::instance().unregisterModule(module_name);
}

switch_bool_t switch_telnyx_kv_module_exists(const char* module_name)
{
	return KVManager::instance().moduleExists(module_name);
}