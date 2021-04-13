import LoginPage from '../../pages/login.f7.html';

import User from '../controllers/users.js';

function checkAuth( { resolve } ) {
  const router = this;
  if (User.isLogged()){
    router.navigate('/', { reloadCurrent: true });
  }
  else {
     if (router.currentRoute.name !== "login"){
       resolve('/login/');
     }
  }
}

const login = [
  {
    name: 'login',
    path: '/login/',
    component: LoginPage,
    beforeEnter: checkAuth,
  },
];

export default login;
