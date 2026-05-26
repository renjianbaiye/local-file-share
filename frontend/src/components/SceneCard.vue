<script setup>
import { ref } from 'vue'
import { usePhotos } from '../composables/usePhotos.js'

const props = defineProps({
  scene: { type: Object, required: true },
})
const emit = defineEmits(['click', 'contextmenu'])
const { getTagLabel } = usePhotos()

const showMenu = ref(false)
const menuPos = ref({ x: 0, y: 0 })

const onContext = (e) => {
  e.preventDefault()
  menuPos.value = { x: e.clientX, y: e.clientY }
  showMenu.value = true
  const close = () => { showMenu.value = false; window.removeEventListener('click', close) }
  setTimeout(() => window.addEventListener('click', close), 0)
}
</script>

<template>
  <article class="scene-card" @click="$emit('click', scene)" @contextmenu="onContext">
    <div class="scene-cover">
      <img :src="scene.cover" :alt="scene.title" loading="lazy" />
      <div class="scene-stack">
        <img
          v-for="(thumb, i) in (scene.thumbnails || []).slice(0, 3)"
          :key="i"
          :src="thumb"
          :style="{ '--stack-i': i }"
          loading="lazy"
        />
      </div>
      <div class="scene-count">{{ scene.photo_count }} 张</div>
    </div>
    <div class="scene-body">
      <h3>{{ scene.title }}</h3>
      <p class="scene-time">{{ scene.time_range }}</p>
      <div class="scene-tags">
        <span v-for="tag in (scene.tags || []).slice(0, 4)" :key="tag">{{ getTagLabel(tag) }}</span>
      </div>
      <div v-if="scene.cluster_score" class="scene-score">
        相关度 {{ (scene.cluster_score * 100).toFixed(0) }}%
      </div>
    </div>

    <!-- Context menu -->
    <Teleport to="body">
      <div v-if="showMenu" class="context-menu" :style="{ left: menuPos.x + 'px', top: menuPos.y + 'px' }">
        <button @click="showMenu = false">重命名</button>
        <button @click="showMenu = false">合并场景</button>
        <button @click="showMenu = false">拆分</button>
        <button @click="showMenu = false">隐藏</button>
      </div>
    </Teleport>
  </article>
</template>
