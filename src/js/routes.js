import AboutPage from '../pages/about.f7.html';

import home from './routes/home.js';
import login from './routes/login.js';
import not_found from './routes/not-found.js';

let routes = [
  {
    name: 'about',
    path: '/about/',
    component: AboutPage,
  },
  ...home,
  ...login,
  ...not_found,
]

export default routes;
