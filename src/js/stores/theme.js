import { createStore } from 'framework7';
import { localForage } from "./../api/config.js";

const theme = createStore({
  state: {
    name: 'dark',
  },
  actions: {
//Mutations
    changeTheme({ state }, name) {
      state.name = name;
    },
//End mutations
    async checkTheme({ dispatch }, selector){
      let theme = await localForage.getItem('theme');
      if (theme == null) theme = 'dark';
      if (theme != 'dark'){ document.querySelector(selector).classList.remove('dark'); }
      dispatch('changeTheme', theme);
    },
    async initTheme({ dispatch }, selector){
      let item = await localForage.getItem('theme');
      if (item != null){
        await dispatch('checkTheme', selector);
      }
    },
    async setTheme({ dispatch }, name){
      await localForage.setItem('theme', name);
      dispatch('changeTheme', name);
    },
  },
  getters: {
    Name({ state }) {
      return state.name;
    },
  },
})

export default theme;
