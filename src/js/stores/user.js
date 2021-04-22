import { createStore } from 'framework7';
import http from './../api/config.js';

const user = createStore({
  state: {
    status: localStorage.getItem('token') ? 'success' : '',
    token: localStorage.getItem('token') || '',
    user: {},
    roles: [],
  },
  actions: {
    logIn({ state, dispatch }, data){
      return new Promise((resolve, reject) => {
        dispatch('auth_request');
        http.post('/user/login', data).then(response => {
          const token = response.data.token;
          delete response.data.token;
          const user = response.data;
          const roles = [1, 4]; //Owner, Customer, Seller, Deliverer//response.data.roles;
          console.log(roles);
          console.log(user);
          localStorage.setItem('token', token);
          http.defaults.headers.common['Authorization'] = token;
          dispatch('auth_success', token, user, roles);
          resolve(response);
        })
        .catch(err => {
          dispatch('auth_error');
          localStorage.removeItem('token');
          reject(err);
        });
      });
    },
    logOut({ state, dispatch }){
      return new Promise((resolve, reject) => {
        dispatch('logout');
        localStorage.removeItem('token')
        http.defaults.headers.common['Authorization'];
        resolve();
      });
    },
//Mutations
    logout({ state }){
      state.status = '';
      state.token = '';
    },
    auth_request({ state }){
      state.status = 'loading';
    },
    auth_success({ state }, token, user, roles){
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
