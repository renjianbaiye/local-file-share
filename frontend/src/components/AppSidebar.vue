<script setup>
import { computed } from 'vue'
import { useRouter, useRoute } from 'vue-router'
import {
  Images, Clapperboard, Users, Inbox, FolderUp, Settings, Sun, Moon,
} from '@lucide/vue'
import { useTheme } from '../composables/useTheme.js'

const router = useRouter()
const route = useRoute()
const { theme, toggle } = useTheme()

const mainNav = [
  { name: '图库', icon: Images, to: '/' },
  { name: '场景', icon: Clapperboard, to: '/scenes' },
  { name: '人物', icon: Users, to: '/people' },
  { name: '待整理', icon: Inbox, to: '/tidy' },
]

const toolNav = [
  { name: '文件传输', icon: FolderUp, to: '/files' },
  { name: '设置', icon: Settings, to: '/settings' },
]

const isActive = (path) => {
  if (path === '/') return route.path === '/'
  return route.path.startsWith(path)
}

const navigate = (path) => router.push(path)
</script>

<template>
  <nav class="app-sidebar">
    <div class="sidebar-brand" @click="navigate('/')">
      <div class="brand-icon">
        <Images :size="18" />
      </div>
      <span class="brand-text">智能相册</span>
    </div>

    <div class="sidebar-nav-main">
      <button
        v-for="item in mainNav"
        :key="item.to"
        class="sidebar-item"
        :class="{ active: isActive(item.to) }"
        @click="navigate(item.to)"
      >
        <component :is="item.icon" :size="18" />
        <span>{{ item.name }}</span>
      </button>
    </div>

    <div class="sidebar-divider" />

    <div class="sidebar-nav-tools">
      <button
        v-for="item in toolNav"
        :key="item.to"
        class="sidebar-item tool-item"
        :class="{ active: isActive(item.to) }"
        @click="navigate(item.to)"
      >
        <component :is="item.icon" :size="16" />
        <span>{{ item.name }}</span>
      </button>
    </div>

    <div class="sidebar-footer">
      <button class="theme-toggle" @click="toggle" :title="theme === 'dark' ? '切换亮色模式' : '切换暗色模式'">
        <Sun v-if="theme === 'dark'" :size="16" />
        <Moon v-else :size="16" />
      </button>
    </div>
  </nav>
</template>
