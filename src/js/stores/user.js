import { createStore } from 'framework7';
import http from './../api/config.js';

const user = createStore({
  state: {
    status: '',
    token: localStorage.getItem('token') || '',
    user: {},
    roles: [],
  },
  actions: {
    logIn({ state, dispatch }, data){
      return new Promise((resolve, reject) => {
        dispatch('auth_request');
        http.post('/user/login', data)
        .then(response => {
          const token = response.data.token;
          const user = response.data.user;
          const roles = response.data.roles;
          localStorage.setItem('token', token);
          localStorage.setItem('roles', roles);
          http.defaults.headers.common['Authorization'] = token;
          dispatch('auth_success', token, user);
          resolve(response);
        })
        .catch(err => {
          dispatch('auth_error');
          localStorage.removeItem('token');
          localStorage.removeItem('roles');
          reject(err);
        });
      });
    },
    logOut(){
      return new Promise((resolve, reject) => {
        dispatch('logout');
        localStorage.removeItem('token');
        localStorage.removeItem('roles');
        http.defaults.headers.common['Authorization'];
        resolve();
      });
    },
//Mutations
    logout({ state }){
      state.status = '';
      state.token = '';
    },
    auth_request(state){
      state.status = 'loading'
    },
    auth_success(state, token, user){
      state.status = 'success'
      state.token = token
      state.user = user
    },
    auth_error(state){
      state.status = 'error'
    },
//End mutations
  },
  getters: {
    isLogged({ state }) {
      return !!state.token;
    },
    authStatus({ state }) {
      return state.status;
    }
  },
})

export default user;
