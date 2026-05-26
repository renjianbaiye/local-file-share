import { createRouter, createWebHistory } from 'vue-router'

const routes = [
  { path: '/', name: 'library', component: () => import('./views/LibraryView.vue') },
  { path: '/scenes', name: 'scenes', component: () => import('./views/ScenesView.vue') },
  { path: '/scenes/:id', name: 'scene-detail', component: () => import('./views/SceneDetailView.vue') },
  { path: '/people', name: 'people', component: () => import('./views/PeopleView.vue') },
  { path: '/tidy', name: 'tidy', component: () => import('./views/TidyView.vue') },
  { path: '/files', name: 'files', component: () => import('./views/FilesView.vue') },
  { path: '/settings', name: 'settings', component: () => import('./views/SettingsView.vue') },
]

const router = createRouter({
  history: createWebHistory(),
  routes,
})

export default router
