<script setup>
import { Sun, Moon, Sparkles, Info } from '@lucide/vue'
import { useTheme } from '../composables/useTheme.js'
import { useEffects } from '../composables/useEffects.js'

const { theme, toggle } = useTheme()
const { effectsEnabled, reducedMotion, toggleEffects } = useEffects()
</script>

<template>
  <div class="view-page settings-view">
    <header class="view-header">
      <h1>设置</h1>
    </header>

    <div class="settings-cards">
      <section class="settings-card">
        <h3><Sun :size="18" /> 外观</h3>
        <div class="setting-row">
          <div>
            <strong>主题模式</strong>
            <p>当前: {{ theme === 'dark' ? '深色' : '浅色' }}</p>
          </div>
          <button class="action-btn" @click="toggle">
            <component :is="theme === 'dark' ? Sun : Moon" :size="15" />
            切换为{{ theme === 'dark' ? '浅色' : '深色' }}
          </button>
        </div>
      </section>

      <section class="settings-card">
        <h3><Sparkles :size="18" /> 动效</h3>
        <div class="setting-row">
          <div>
            <strong>照片动效</strong>
            <p>控制精选回忆流水效果和场景详情氛围粒子</p>
            <p v-if="reducedMotion" class="setting-note">系统已开启"减少动态效果"，动效自动禁用</p>
          </div>
          <button class="action-btn" @click="toggleEffects" :disabled="reducedMotion">
            {{ effectsEnabled ? '关闭动效' : '开启动效' }}
          </button>
        </div>
      </section>

      <section class="settings-card">
        <h3><Info :size="18" /> 关于</h3>
        <div class="setting-row">
          <div>
            <strong>本地智能相册</strong>
            <p>AI 驱动的照片整理系统</p>
            <p class="setting-note">DINOv2-Large Tagger v3 · 42 标签体系 · Vue 3 + Vite</p>
          </div>
        </div>
      </section>
    </div>
  </div>
</template>
