import { dbConfig } from './key-base';
import * as localforage from "localforage";
import CryptoJS from 'crypto-js';

localforage.config(dbConfig);

const localForage = {
  /**
  * Set object in storage
  * @param {string} key object identifier
  * @param {any} data object to store
  * @return {void}
  */
  async setItem(key, data) {
    data = CryptoJS.AES.encrypt(JSON.stringify(data), dbConfig.key).toString();
    return localforage.setItem(key, data);
  },

  /**
  * Get object from storage
  * @param {string} key object identifier
  * @return {any}
  */
  async getItem(key) {
    const data = await localforage.getItem(key);
    if (data == null){
      return data;
    }
    const bytes = CryptoJS.AES.decrypt(data, dbConfig.key);
    return JSON.parse(bytes.toString(CryptoJS.enc.Utf8));
  },

  /**
  * Delete object from storage
  * @param {string} key object identifier
  */
  async deleteItem(key) {
    return localforage.removeItem(key);
  },

  /**
  * Clear storage
  */
  async deleteAll() {
    return localforage.clear();
  },
};

export { localForage };
