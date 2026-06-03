<script setup>
import { useRouter, useRoute } from 'vue-router'
import {
  Images, Clapperboard, Users, Inbox, FolderUp, Settings, Sun, Moon,
} from '@lucide/vue'
import { useTheme } from '../composables/useTheme.js'

const router = useRouter()
const route = useRoute()
const { theme, toggle } = useTheme()

const libraryNav = [
  { name: '图库', icon: Images, to: '/' },
  { name: '场景', icon: Clapperboard, to: '/scenes' },
  { name: '人物', icon: Users, to: '/people' },
  { name: '待整理', icon: Inbox, to: '/tidy' },
]

const utilityNav = [
  { name: '传输', icon: FolderUp, to: '/files' },
  { name: '设置', icon: Settings, to: '/settings' },
]

const isActive = (path) => {
  if (path === '/') return route.path === '/'
  return route.path.startsWith(path)
}

const navigate = (path) => router.push(path)
</script>

<template>
  <aside class="app-sidebar" aria-label="主导航">
    <button class="sidebar-brand" type="button" title="智能相册" @click="navigate('/')">
      <span class="brand-mark">
        <Images :size="18" />
      </span>
      <span class="brand-line brand-line-dark"></span>
      <span class="brand-line"></span>
      <span class="brand-line short"></span>
    </button>

    <nav class="sidebar-section" aria-label="图库导航">
      <button
        v-for="item in libraryNav"
        :key="item.to"
        class="sidebar-item"
        :class="{ active: isActive(item.to) }"
        type="button"
        :title="item.name"
        @click="navigate(item.to)"
      >
        <component :is="item.icon" :size="18" :stroke-width="isActive(item.to) ? 2.2 : 1.8" />
        <span>{{ item.name }}</span>
      </button>
    </nav>

    <nav class="sidebar-section sidebar-section-tools" aria-label="工具导航">
      <button
        v-for="item in utilityNav"
        :key="item.to"
        class="sidebar-item"
        :class="{ active: isActive(item.to) }"
        type="button"
        :title="item.name"
        @click="navigate(item.to)"
      >
        <component :is="item.icon" :size="17" :stroke-width="1.8" />
        <span>{{ item.name }}</span>
      </button>
    </nav>

    <button
      class="theme-toggle"
      type="button"
      @click="toggle"
      :title="theme === 'dark' ? '浅色模式' : '深色模式'"
    >
      <Sun v-if="theme === 'dark'" :size="17" />
      <Moon v-else :size="17" />
      <span>{{ theme === 'dark' ? '浅色' : '深色' }}</span>
    </button>
  </aside>
</template>
