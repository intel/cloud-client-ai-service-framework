import Vue from 'vue'
import VueRouter from 'vue-router'
import Scan from '../components/Scan.vue'
import Photo from '../components/Photo.vue'

const NotFound = { template: "<div>Page not found</div>" };

Vue.use(VueRouter)

const router = new VueRouter({
  routes: [
    { path: '/',      redirect: '/Scan' },
    { path: '/scan',  component: Scan },
    { path: '/photo/:type/:id?', component: Photo },
    { path: '*',      component: NotFound }
  ]
})

export default router

