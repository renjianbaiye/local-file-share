<script setup>
import { computed, ref, onMounted } from 'vue'
import { useRoute, useRouter } from 'vue-router'
import { ChevronLeft } from '@lucide/vue'
import { usePhotos } from '../composables/usePhotos.js'
import PhotoMasonry from '../components/PhotoMasonry.vue'
import PhotoPreviewModal from '../components/PhotoPreviewModal.vue'
import PetalOverlay from '../components/effects/PetalOverlay.vue'

const route = useRoute()
const router = useRouter()
const { scenes, photos, load, loaded, getTagLabel } = usePhotos()
const previewItem = ref(null)

onMounted(() => load())

const scene = computed(() => scenes.value.find(s => s.id === route.params.id))

const scenePhotos = computed(() => {
  if (!scene.value?.photo_ids) return photos.value.slice(0, 12)
  const ids = new Set(scene.value.photo_ids)
  const matched = photos.value.filter(p => ids.has(p.id))
  return matched.length > 0 ? matched : photos.value.slice(0, 8)
})

const overlayType = computed(() => {
  const tags = scene.value?.tags || []
  if (tags.some(t => ['beach', 'sea_or_lake', 'river_or_water'].includes(t))) return 'sparkle'
  if (tags.some(t => ['park', 'scenery', 'travel_checkin', 'nature'].includes(t))) return 'petal'
  if (tags.includes('night')) return 'sparkle'
  if (tags.some(t => ['snow', 'rain_or_snow'].includes(t))) return 'snow'
  return 'none'
})
</script>

<template>
  <div class="view-page scene-detail-view">
    <button class="back-btn" @click="router.push('/scenes')">
      <ChevronLeft :size="18" />
      返回场景
    </button>

    <div v-if="scene" class="scene-hero">
      <PetalOverlay :type="overlayType" :density="12" :opacity="0.15" />
      <img :src="scene.cover" :alt="scene.title" class="hero-img" />
      <div class="hero-content">
        <h1>{{ scene.title }}</h1>
        <p>{{ scene.time_range }} · {{ scene.photo_count }} 张照片</p>
        <div class="hero-tags">
          <span v-for="tag in (scene.tags || []).slice(0, 5)" :key="tag" class="tag-pill">
            {{ getTagLabel(tag) }}
          </span>
        </div>
      </div>
    </div>

    <PhotoMasonry :photos="scenePhotos" @preview="previewItem = $event" />

    <PhotoPreviewModal
      :item="previewItem"
      :items="scenePhotos"
      @close="previewItem = null"
      @navigate="previewItem = $event"
    />
  </div>
</template>
