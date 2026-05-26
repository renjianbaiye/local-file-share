<script setup>
import { Sparkles } from '@lucide/vue'

defineProps({
  icon: { type: Object, default: () => Sparkles },
  title: { type: String, default: '暂无内容' },
  description: { type: String, default: '' },
})
</script>

<template>
  <div class="empty-state">
    <div class="empty-icon">
      <component :is="icon" :size="40" />
    </div>
    <strong>{{ title }}</strong>
    <p v-if="description">{{ description }}</p>
    <slot />
  </div>
</template>
