import { createStore } from 'framework7';
import { http, localForage } from './../api/config.js';

const user = createStore({
  state: {
    status: '',
    token: '',
    user: {},
    roles: [],
    location: {},
  },
  actions: {
    async setBasicData({ dispatch }, { token, user, roles, location }){
      await localForage.setItem('user', user);
      await localForage.setItem('token', token);
      await localForage.setItem('roles', roles);
      await localForage.setItem('location', location);
      dispatch('auth_success', { token, user, roles, location });
    },
    async getBasicData({ dispatch }){
      const result = {
        user: await localForage.getItem('user'),
        token: await localForage.getItem('token'),
        roles: await localForage.getItem('roles'),
        location: await localForage.getItem('location'),
      }
      dispatch('auth_success', result);
    },
    async clearBasicData(){
      await localForage.deleteItem('user');
      await localForage.deleteItem('token');
      await localForage.deleteItem('roles');
      await localForage.deleteItem('location');
    },
    async checkData({ dispatch }){
      let token = await localForage.getItem('token');
      if (token != null){
        await dispatch('getBasicData');
      }
    },
    async logIn({ dispatch }, data){
      dispatch('auth_request');
      try {
        const response = await http.post('/user/login', data);

        http.defaults.headers.common['Authorization'] = response.data.token;
        await dispatch('setBasicData', response.data );

        delete response.data.token;
        delete response.data.roles;
        delete response.data.location;

        return response;
      }
      catch (error) {
        dispatch('auth_error');
        await clearBasicData();
        throw new error;
      }
    },
    async logOut({ dispatch }){
      dispatch('logout');
      await dispatch('clearBasicData');
      delete http.defaults.headers.common['Authorization'];
    },
//Mutations
    logout({ state }){
      state.status = '';
      state.token = '';
      state.user = {};
      state.roles = [];
      state.location = {};
    },
    auth_request({ state }){
      state.status = 'loading';
    },
    auth_success({ state }, { token, user, roles, location }){
      state.status = 'success';
      state.token = token;
      state.user = user;
      state.roles = roles;
      state.location = location;
    },
    auth_error({ state }){
      state.status = 'error'
    },
//End mutations
  },
  getters: {
    isLogged({ state }) {
      return !!state.token;
    },
    token({ state }) {
      return state.token;
    },
    authStatus({ state }) {
      return state.status;
    },
    roles({ state }){
      return state.roles;
    },
    location({ state }){
      return state.location
    },
    displayName({ state }){
      return state.user;
    }
  },
})

export default user;
