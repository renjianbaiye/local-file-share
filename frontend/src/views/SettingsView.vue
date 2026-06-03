<script setup>
import { Sun, Moon, Sparkles, Info, Monitor } from '@lucide/vue'
import { useTheme } from '../composables/useTheme.js'
import { useEffects } from '../composables/useEffects.js'

const { theme, toggle } = useTheme()
const { effectsEnabled, reducedMotion, canAnimate, toggleEffects } = useEffects()
</script>

<template>
  <div class="view-page settings-view">
    <header class="view-header">
      <h1>设置</h1>
      <p>个性化你的智能相册体验</p>
    </header>

    <div class="settings-cards">
      <section class="settings-card">
        <h3><Monitor :size="16" /> 外观</h3>
        <div class="setting-row">
          <div>
            <strong>主题模式</strong>
            <p>当前使用 {{ theme === 'dark' ? '深色' : '浅色' }} 模式</p>
          </div>
          <button class="toggle-chip" @click="toggle">
            <component :is="theme === 'dark' ? Sun : Moon" :size="14" />
            <span>切换为{{ theme === 'dark' ? '浅色' : '深色' }}</span>
          </button>
        </div>
      </section>

      <section class="settings-card">
        <h3><Sparkles :size="16" /> 动效与动画</h3>
        <div class="setting-row">
          <div>
            <strong>环境动效</strong>
            <p>控制背景环境光、照片流动效和场景氛围粒子。</p>
            <p v-if="reducedMotion" class="setting-note">系统已开启“减少动态效果”，动效会自动停用。</p>
          </div>
          <label class="toggle-switch" :class="{ disabled: reducedMotion }">
            <input
              type="checkbox"
              :checked="effectsEnabled"
              :disabled="reducedMotion"
              @change="toggleEffects"
            />
            <span class="toggle-track">
              <span class="toggle-knob"></span>
            </span>
          </label>
        </div>
        <div class="setting-row status-row">
          <div>
            <strong>当前状态</strong>
            <p>
              动效系统:
              <span :style="{ color: canAnimate ? 'var(--green)' : 'var(--text-tertiary)' }">
                {{ canAnimate ? '已启用' : '已停用' }}
              </span>
            </p>
          </div>
        </div>
      </section>

      <section class="settings-card">
        <h3><Info :size="16" /> 关于</h3>
        <div class="setting-row">
          <div>
            <strong>本地智能相册</strong>
            <p>本地照片整理与浏览系统</p>
            <p class="setting-note about-note">
              DINOv2-Large Tagger v3 · 42 标签体系 · Vue 3 + Vite
            </p>
          </div>
        </div>
      </section>
    </div>
  </div>
</template>

<style scoped>
.toggle-switch {
  position: relative;
  display: inline-block;
  cursor: pointer;
  flex-shrink: 0;
}

.toggle-switch.disabled {
  opacity: 0.4;
  pointer-events: none;
}

.toggle-switch input {
  position: absolute;
  opacity: 0;
  width: 0;
  height: 0;
}

.toggle-track {
  display: block;
  width: 51px;
  height: 31px;
  border-radius: 31px;
  background: var(--bg-glass-strong);
  border: 1px solid var(--border-glass);
  position: relative;
  transition: background 0.3s, border-color 0.3s;
}

.toggle-switch input:checked + .toggle-track {
  background: var(--green);
  border-color: var(--green);
}

.toggle-knob {
  position: absolute;
  top: 2px;
  left: 2px;
  width: 25px;
  height: 25px;
  border-radius: 50%;
  background: white;
  box-shadow: 0 2px 4px rgba(0,0,0,0.15);
  transition: transform 0.25s cubic-bezier(0.34, 1.56, 0.64, 1);
}

.toggle-switch input:checked + .toggle-track .toggle-knob {
  transform: translateX(20px);
}

.toggle-chip {
  display: inline-flex;
  align-items: center;
  gap: 6px;
  padding: 7px 16px;
  border-radius: var(--radius-full);
  background: var(--bg-glass);
  border: 1px solid var(--border-glass);
  font-size: 13px;
  font-weight: 500;
  color: var(--text-primary);
  transition: all 0.2s;
  white-space: nowrap;
  flex-shrink: 0;
}

.toggle-chip:hover {
  background: var(--bg-glass-hover);
  box-shadow: var(--shadow-sm);
}

.toggle-chip:active {
  transform: scale(0.97);
}

.status-row {
  border-top: 1px solid var(--border-subtle);
}

.about-note {
  color: var(--text-tertiary) !important;
  font-style: normal;
}
</style>
