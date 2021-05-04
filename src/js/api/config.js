import { url } from './key';
import { localForage } from './encrypt';
import axios from 'axios';

const http = axios.create({
  baseURL: `${url}`,
});

async function checkToken (){
  let token = await localForage.getItem('token');
  if (token != null){
    http.defaults.headers.common['Authorization'] = token;
  }
};

checkToken();

export { http, localForage };