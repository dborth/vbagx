import HomePage from '../../pages/home.f7.html';
import LeftPanelPage from '../../pages/home/panel-left.f7.html';
import RightPanelPage from '../../pages/home/panel-right.f7.html';

const home = [
  {
    name: 'home',
    path: '/',
    component: HomePage,
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
