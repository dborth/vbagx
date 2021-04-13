import UserEditPage from '../../pages/user/edit-data.f7.html';
import UserNewPage from '../../pages/user/new-data.f7.html';
import UserResetPage from '../../pages/user/reset-data.f7.html';

import Check from '../controllers/check';

const users = [
  {
    name: 'user-edit',
    path: '/user-edit/',
    popup:{
      component: UserEditPage,
    },
    beforeEnter: [Check.authorization],
  },
  {
    name: 'user-new',
    path: '/user-new/',
    popup:{
      component: UserNewPage,
    },
  },
  {
    name: 'user-reset',
    path: '/user-reset/',
    popup:{
      component: UserResetPage,
    },
  },
];

export default users;
