/**
 * Storage Manager
 * Управление localStorage для сохранения данных
 */

class StorageManager {
    constructor() {
        this.prefix = 'smart_hammer_';
    }

    /**
     * Получить ключ для хранения
     */
    getKey(key) {
        return this.prefix + key;
    }

    /**
     * Сохранить данные в localStorage
     */
    save(key, data) {
        try {
            const serializedData = JSON.stringify(data);
            localStorage.setItem(this.getKey(key), serializedData);
            return true;
        } catch (e) {
            console.error('Ошибка сохранения в localStorage:', e);
            return false;
        }
    }

    /**
     * Загрузить данные из localStorage
     */
    load(key) {
        try {
            const serializedData = localStorage.getItem(this.getKey(key));
            if (serializedData === null) {
                return null;
            }
            return JSON.parse(serializedData);
        } catch (e) {
            console.error('Ошибка загрузки из localStorage:', e);
            return null;
        }
    }

    /**
     * Удалить данные из localStorage
     */
    remove(key) {
        try {
            localStorage.removeItem(this.getKey(key));
            return true;
        } catch (e) {
            console.error('Ошибка удаления из localStorage:', e);
            return false;
        }
    }

    /**
     * Очистить все данные с префиксом
     */
    clearAll() {
        try {
            const keys = Object.keys(localStorage);
            for (let i = 0; i < keys.length; i++) {
                const key = keys[i];
                if (key.startsWith(this.prefix)) {
                    localStorage.removeItem(key);
                }
            }
            return true;
        } catch (e) {
            console.error('Ошибка очистки localStorage:', e);
            return false;
        }
    }

    /**
     * Сохранение чатов
     */
    saveChats(chats) {
        return this.save('chats', chats);
    }

    /**
     * Загрузка чатов
     */
    getChats() {
        return this.load('chats');
    }

    /**
     * Сохранение сессий
     */
    saveSessions(sessions) {
        return this.save('sessions', sessions);
    }

    /**
     * Загрузка сессий
     */
    getSessions() {
        return this.load('sessions');
    }

    /**
     * Сохранение текущей сессии
     */
    saveCurrentSession(session) {
        return this.save('current_session', session);
    }

    /**
     * Загрузка текущей сессии
     */
    getCurrentSession() {
        return this.load('current_session');
    }

    /**
     * Сохранение истории
     */
    saveHistory(history) {
        return this.save('history', history);
    }

    /**
     * Загрузка истории
     */
    getHistory() {
        return this.load('history');
    }

    /**
     * Сохранение настроек
     */
    saveSettings(settings) {
        return this.save('settings', settings);
    }

    /**
     * Загрузка настроек
     */
    getSettings() {
        return this.load('settings');
    }

    /**
     * Сохранение конфигурации
     */
    saveConfig(config) {
        return this.save('config', config);
    }

    /**
     * Загрузка конфигурации
     */
    getConfig() {
        return this.load('config');
    }

    /**
     * Экспорт конфигурации
     */
    exportConfig() {
        const config = this.getConfig();
        if (config) {
            const dataStr = "data:text/json;charset=utf-8," + encodeURIComponent(JSON.stringify(config, null, 2));
            const downloadAnchorNode = document.createElement('a');
            downloadAnchorNode.setAttribute("href", dataStr);
            downloadAnchorNode.setAttribute("download", "smart_hammer_config.json");
            document.body.appendChild(downloadAnchorNode);
            downloadAnchorNode.click();
            downloadAnchorNode.remove();
            return config;
        }
        return null;
    }

    /**
     * Импорт конфигурации
     */
    importConfig(config) {
        if (config) {
            return this.saveConfig(config);
        }
        return false;
    }

    /**
     * Сохранение истории сообщений
     */
    saveMessages(sessionId, messages) {
        return this.save('messages_' + sessionId, messages);
    }

    /**
     * Загрузка истории сообщений
     */
    getMessages(sessionId) {
        return this.load('messages_' + sessionId);
    }

    /**
     * Сохранение временных данных
     */
    saveTemp(key, data, ttl = 3600000) {
        try {
            const dataWithTTL = {
                data: data,
                ttl: Date.now() + ttl
            };
            localStorage.setItem(this.getKey('temp_' + key), JSON.stringify(dataWithTTL));
            return true;
        } catch (e) {
            console.error('Ошибка сохранения временных данных:', e);
            return false;
        }
    }

    /**
     * Загрузка временных данных
     */
    getTemp(key) {
        try {
            const serializedData = localStorage.getItem(this.getKey('temp_' + key));
            if (serializedData === null) {
                return null;
            }
            const data = JSON.parse(serializedData);
            
            // Проверка TTL
            if (Date.now() > data.ttl) {
                this.removeTemp(key);
                return null;
            }
            
            return data.data;
        } catch (e) {
            console.error('Ошибка загрузки временных данных:', e);
            return null;
        }
    }

    /**
     * Удаление временных данных
     */
    removeTemp(key) {
        try {
            localStorage.removeItem(this.getKey('temp_' + key));
            return true;
        } catch (e) {
            console.error('Ошибка удаления временных данных:', e);
            return false;
        }
    }

    /**
     * Очистка всех временных данных
     */
    clearTemp() {
        try {
            const keys = Object.keys(localStorage);
            for (let i = 0; i < keys.length; i++) {
                const key = keys[i];
                if (key.startsWith(this.getKey('temp_'))) {
                    localStorage.removeItem(key);
                }
            }
            return true;
        } catch (e) {
            console.error('Ошибка очистки временных данных:', e);
            return false;
        }
    }

    /**
     * Проверка: существует ли ключ
     */
    has(key) {
        return localStorage.getItem(this.getKey(key)) !== null;
    }

    /**
     * Получить размер хранилища
     */
    getStorageSize() {
        try {
            const key = '__storage_size__';
            const size = localStorage.getItem(key);
            if (size !== null) {
                localStorage.removeItem(key);
            }
            localStorage.setItem(key, btoa(JSON.stringify(localStorage)));
            const data = localStorage.getItem(key);
            localStorage.removeItem(key);
            return atob(data).length;
        } catch (e) {
            console.error('Ошибка получения размера хранилища:', e);
            return 0;
        }
    }

    /**
     * Проверка: достаточно ли места в хранилище
     */
    hasEnoughSpace(size) {
        const currentSize = this.getStorageSize();
        const maxSize = 5 * 1024 * 1024; // 5MB
        return currentSize + size <= maxSize;
    }
}

// Экспорт для использования в других модулях
if (typeof module !== 'undefined' && module.exports) {
    module.exports = StorageManager;
}
