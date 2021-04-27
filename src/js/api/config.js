import { url, dbConfig } from './key';
import axios from 'axios';
import * as localForage from "localforage";

const http = axios.create({
  baseURL: `${url}`,
});

localForage.config(dbConfig);

async function checkToken (){
  let token = await localForage.getItem('token');
  if (token != null){
    http.defaults.headers.common['Authorization'] = token;
  }
};

checkToken();

export { http, localForage };