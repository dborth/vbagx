import url from './key';
import axios from 'axios';

const http = axios.create({
  baseURL: `${url}`,
});

const token = localStorage.getItem('token')

if (token) {
  http.defaults.headers.common['Authorization'] = token;
}

export default http;