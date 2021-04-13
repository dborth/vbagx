import NotFoundPage from '../../pages/404.f7.html';

/*Must be in last position*/
const not_found = [
  {
    name: 'not_found',
    path: '(.*)',
    component: NotFoundPage
  }
];

export default not_found;
