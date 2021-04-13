import url from './key';
import axios from 'axios';

const HTTP = axios.create({
  baseURL: `${url}`,
});

const token = localStorage.getItem('user-token')
if (token) {
  HTTP.defaults.headers.common['Authorization'] = token
}

export default HTTP;