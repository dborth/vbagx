import HomePage from '../../pages/home.f7.html';
import LeftPanelPage from '../../pages/home/panel-left.f7.html';
import RightPanelPage from '../../pages/home/panel-right.f7.html';

import Check from './../controllers/check.js';

const home = [
  {
    name: 'home',
    path: '/',
    component: HomePage,
    allowedRoles: [1,9,10,14,15],
    beforeEnter: [Check.authorization, Check.permission],
  },
  {
    name: 'panel-left-',
    path: '/panel-left/',
    panel: {
      component: LeftPanelPage,
    }
  },
  {
    name: 'panel-right',
    path: '/panel-right/',
    panel: {
      component: RightPanelPage,
    }
  },
];

export default home;
