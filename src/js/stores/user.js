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
      dispatch('auth_success', { token, user, roles});
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
      await localForage.deleteItem('user');
      await localForage.deleteItem('token');
      await localForage.deleteItem('roles');
    },
    async checkData({ dispatch }){
      let token = await localForage.getItem('token');
      if (token != null){
        let data = this.decodeJWT(token);
        let today = new Date(); //Current date
        let expirationDate = new Date(data.expiration);

        if (today.getTime() < expirationDate.getTime()){
          await dispatch('getBasicData');
        }
        else{
          await dispatch('logOut');
        }
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
    decodeJWT (token) {
      let base64Url = token.split('.')[1];
      let base64 = base64Url.replace(/-/g, '+').replace(/_/g, '/');
      let jsonPayload = decodeURIComponent(atob(base64).split('').map(function(c) {
          return '%' + ('00' + c.charCodeAt(0).toString(16)).slice(-2);
      }).join(''));

      return JSON.parse(jsonPayload);
    },
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
