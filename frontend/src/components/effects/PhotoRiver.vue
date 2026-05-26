<script setup>
import { ref, computed, onMounted, onBeforeUnmount } from 'vue'
import { useEffects } from '../../composables/useEffects.js'

const props = defineProps({
  photos: { type: Array, default: () => [] },
  direction: { type: String, default: 'left' },
  speed: { type: Number, default: 30 },
  height: { type: Number, default: 200 },
  paused: { type: Boolean, default: false },
})

const { canAnimate } = useEffects()
const containerRef = ref(null)
const isHovered = ref(false)

// Duplicate photos for seamless loop
const displayPhotos = computed(() => {
  if (!props.photos.length) return []
  // Need at least enough to fill the container + overflow
  const base = props.photos
  return [...base, ...base, ...base]
})

const animDuration = computed(() => {
  const count = props.photos.length || 1
  return (count * 220) / props.speed
})

const shouldAnimate = computed(() => canAnimate.value && !props.paused && !isHovered.value)

const animClass = computed(() => {
  if (!canAnimate.value) return ''
  return `river-${props.direction}`
})
</script>

<template>
  <div
    ref="containerRef"
    class="photo-river"
    :class="{ paused: !shouldAnimate }"
    :style="{ height: height + 'px' }"
    @mouseenter="isHovered = true"
    @mouseleave="isHovered = false"
  >
    <div
      class="river-track"
      :class="animClass"
      :style="{ animationDuration: animDuration + 's' }"
    >
      <div
        v-for="(photo, i) in displayPhotos"
        :key="i"
        class="river-photo"
      >
        <img :src="photo" :alt="`photo-${i}`" loading="lazy" />
      </div>
    </div>
  </div>
</template>
