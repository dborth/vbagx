import { createStore } from 'framework7';

const theme = createStore({
  state: {
    name: 'theme-dark',
  },
  actions: {
    changeTheme({ state }, name) {
      state.name = name;
    },
  },
  getters: {
    Name({ state }) {
      return state.name;
    }
  },
})

export default theme;
