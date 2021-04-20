
import { createStore } from 'framework7';

const store = createStore({
  state: {
    theme: {
      name: 'theme-dark',
    }
  },
  actions: {
    changeTheme({ state }, name) {
      state.theme.name = name;
    },
  },
  getters: {
    themeName({ state }) {
      return state.theme.name;
    }
  },
})
export default store;
