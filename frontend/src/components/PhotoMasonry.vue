<script setup>
import { ref, onMounted, onBeforeUnmount } from 'vue'
import { usePhotos } from '../composables/usePhotos.js'
import { Check } from '@lucide/vue'

const props = defineProps({
  photos: { type: Array, default: () => [] },
  columns: { type: Number, default: 0 },
})
const emit = defineEmits(['preview'])

const { getTagLabel, formatDay, isSelectMode, selectedIds, toggleSelection } = usePhotos()
const containerRef = ref(null)
const visiblePhotos = ref([])
const observedSet = new Set()
let observer = null

const setupObserver = () => {
  if (observer) observer.disconnect()
  observer = new IntersectionObserver((entries) => {
    for (const entry of entries) {
      if (entry.isIntersecting) {
        const id = entry.target.dataset.photoId
        if (id && !visiblePhotos.value.includes(id)) {
          visiblePhotos.value.push(id)
        }
      }
    }
  }, { rootMargin: '200px' })
}

const observeItems = () => {
  if (!containerRef.value) return
  const items = containerRef.value.querySelectorAll('.masonry-item')
  items.forEach(el => {
    if (!observedSet.has(el)) {
      observedSet.add(el)
      observer?.observe(el)
    }
  })
}

onMounted(() => {
  setupObserver()
  // Initial observe after render
  setTimeout(observeItems, 100)
})

onBeforeUnmount(() => {
  observer?.disconnect()
})

const isVisible = (photo) => visiblePhotos.value.includes(String(photo.id))

const topTags = (photo) => {
  if (!photo.tags) return []
  return photo.tags
    .filter(t => t.predicted || t.derived)
    .sort((a, b) => (b.probability || 0) - (a.probability || 0))
    .slice(0, 3)
}

const handleClick = (photo) => {
  if (isSelectMode.value) toggleSelection(photo.id)
  else emit('preview', photo)
}

// Re-observe when photos change
import { watch, nextTick } from 'vue'
watch(() => props.photos, async () => {
  await nextTick()
  observeItems()
}, { deep: false })
</script>

<template>
  <div ref="containerRef" class="photo-masonry" :style="columns ? { '--masonry-cols': columns } : {}">
    <div
      v-for="(photo, idx) in photos"
      :key="photo.id"
      class="masonry-item"
      :class="{ 'is-selected': selectedIds.has(photo.id) }"
      :data-photo-id="photo.id"
      :style="{ '--i': idx }"
      @click="handleClick(photo)"
    >
      <div class="masonry-img-wrap">
        <img
          v-if="isVisible(photo)"
          :src="photo.thumbnailUrl"
          :alt="photo.fileName"
          loading="lazy"
        />
        <div v-else class="masonry-placeholder" />
        
        <div v-if="isSelectMode" class="select-indicator" :class="{ selected: selectedIds.has(photo.id) }">
          <Check v-if="selectedIds.has(photo.id)" :size="14" />
        </div>
        <div class="masonry-overlay">
          <div class="masonry-tags" v-if="topTags(photo).length">
            <span v-for="t in topTags(photo)" :key="t.tag">{{ getTagLabel(t.tag) }}</span>
          </div>
          <div class="masonry-info">
            <span v-if="photo.qualityScore" class="quality-badge">{{ (photo.qualityScore * 100).toFixed(0) }}</span>
          </div>
        </div>
      </div>
    </div>
  </div>
</template>
