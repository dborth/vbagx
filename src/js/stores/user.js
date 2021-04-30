import { createStore } from 'framework7';
import { http, localForage } from './../api/config.js';

const user = createStore({
  state: {
    status: '',
    token: '',
    user: {},
    roles: [],
  },
  actions: {
    async setBasicData({ dispatch }, { token, user, roles }){
      await localForage.setItem('user', user);
      await localForage.setItem('token', token);
      await localForage.setItem('roles', roles);
      dispatch('auth_success', { token, user, roles });
    },
    async getBasicData({ dispatch }){
      const result = {
        user: await localForage.getItem('user'),
        token: await localForage.getItem('token'),
        roles: await localForage.getItem('roles'),
      }
      dispatch('auth_success', result);
    },
    async clearBasicData(){
      await localForage.removeItem('user');
      await localForage.removeItem('token');
      await localForage.removeItem('roles');
    },
    async checkData({ dispatch }){
      let token = await localForage.getItem('token');
      if (token != null){
        await dispatch('getBasicData');
      }
    },
    async logIn({ state, dispatch }, data){
      dispatch('auth_request');
      try {
        const response = await http.post('/user/login', data);
        const token = response.data.token;
        delete response.data.token;
        const user = response.data.user;
        const roles = response.data.roles; //[1, 4, 14]; //Owner, Customer, Seller, Deliverer
        http.defaults.headers.common['Authorization'] = token;
        await dispatch('setBasicData', { token, user, roles });
        return response;
      } catch (error) {
        dispatch('auth_error');
        await clearBasicData();
        throw new error;
      }
    },
    async logOut({ state, dispatch }){
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
    },
    auth_request({ state }){
      state.status = 'loading';
    },
    auth_success({ state }, { token, user, roles }){
      state.status = 'success';
      state.token = token;
      state.user = user;
      state.roles = roles;
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
    displayName({ state }){
      return state.user;
    }
  },
})

export default user;
