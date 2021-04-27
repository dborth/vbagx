import { createStore } from 'framework7';
import $ from 'dom7';
import { localForage } from "./../api/config.js";

const theme = createStore({
  state: {
    name: 'theme-dark',
  },
  actions: {
//Mutations
    changeTheme({ state }, name) {
      state.name = name;
    },
//End mutations
    async checkTheme({ state, dispatch }, selector){
      let theme = await localForage.getItem('theme');
      if (theme != 'theme-dark'){ $(selector).removeClass('theme-dark'); }
      dispatch('changeTheme', theme);
    },
    async initTheme({state, dispatch}, selector){
      let item = await localForage.getItem('theme');
      if (item != null){
        await dispatch('checkTheme', selector);
      }
    },
    async setTheme({ state, dispatch }, name){
      await localForage.setItem('theme', name);
      dispatch('changeTheme', name);
    },
  },
  getters: {
    Name({ state }) {
      return state.name;
    }
  },
})

export default theme;
