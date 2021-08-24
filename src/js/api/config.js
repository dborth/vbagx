import { url } from './key';
import { localForage } from './encrypt';
import axios from 'axios';

const http = axios.create({
  baseURL: `${url}`,
  validateStatus: status => (status => 200 && status < 300) || (status => 400 && status < 500),
});

async function checkToken (){
  let token = await localForage.getItem('token');
  if (token != null){
    http.defaults.headers.common['Authorization'] = token;
  }
};

checkToken();

export { http, localForage };
