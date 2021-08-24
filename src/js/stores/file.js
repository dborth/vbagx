import { createStore } from 'framework7';
import { http } from './../api/config.js';

const file = createStore({
  state: {
    item: {},
    list: [],
  },
  actions: {
//Mutations
    insert_list_data({ state }, data) {
      state.list = data;
    },
    set_item({ state}, data){
      state.item = data;
    },
//End mutations
    async getFileItem({ dispatch }, param){
      let response = [];
      let result = {};

      result = await http.get(`file/get/${param}`);
      if (result.status === 200){
        response = result.data;
        response.FilePath = `${http.defaults.baseURL}/${response.FilePath}/${response.FileName}`;
        dispatch('set_item', response);
        return response;
      }
      else{
        return result;
      }
    },
    async getFileRequestOrder({ dispatch }, param){
      let response = [];
      let result = {};

      result = await http.get(`file/requestorder/${param}`);
      if (result.status === 200){
        response = result.data;
        dispatch('set_item', response);
        return response;
      }
      else{
        return result;
      }
    },
    async getFileRequestOrderItem({ dispatch }, param){
      let response = [];
      let result = {};

      result = await http.get(`file/requestorder/${param}`);
      if (result.status === 200){
        response = result.data;
        dispatch('insert_list_data', response);
        return response;
      }
      else{
        return result;
      }
    },
    async uploadFileRequestOrder({ dispatch }, item){
      let response = [];
      let result = {};

      result = await http.post(`file/upload/requestorder/single`, item);
      if (result.status === 201){
        response = result.data;
        return response;
      }
      else{
        return result;
      }
    },
    async getFileBatch({ dispatch }, params){

    },
    async uploadFileItem({ dispatch }, params){

    },
    async uploadFileBatch({ dispatch }, params){

    },
  },
  getters: {
    Item({ state }) {
      return state.name;
    }
  },
});

export default file;
