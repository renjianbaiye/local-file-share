<script setup>
import { computed } from 'vue'
import { usePhotos } from '../composables/usePhotos.js'
import { CheckSquare } from '@lucide/vue'

const props = defineProps({
  date: { type: String, required: true },
  photos: { type: Array, required: true }
})

const emit = defineEmits(['preview'])

const { isSelectMode, selectedIds, toggleSelection } = usePhotos()

const heroPhoto = computed(() => props.photos[0])
const tilePhotos = computed(() => props.photos.slice(1))

const handlePhotoClick = (photo) => {
  if (isSelectMode.value) {
    toggleSelection(photo.id)
  } else {
    emit('preview', photo)
  }
}
</script>

<template>
  <section class="timeline-section">
    <h2 class="timeline-date">{{ date }}</h2>
    <div class="timeline-grid">
      <!-- First photo is the hero/cover -->
      <div
        v-if="heroPhoto"
        class="timeline-hero"
        :class="{ 'is-selected': isSelectMode && selectedIds.has(heroPhoto.id) }"
        @click="handlePhotoClick(heroPhoto)"
      >
        <img :src="heroPhoto.thumbnailUrl" :alt="heroPhoto.fileName" loading="lazy" />
        <div v-if="isSelectMode" class="select-indicator" :class="{ selected: selectedIds.has(heroPhoto.id) }">
          <CheckSquare v-if="selectedIds.has(heroPhoto.id)" :size="14" />
        </div>
      </div>
      
      <div class="timeline-tiles">
        <div
          v-for="photo in tilePhotos"
          :key="photo.id"
          class="timeline-tile"
          :class="{ 'is-selected': isSelectMode && selectedIds.has(photo.id) }"
          @click="handlePhotoClick(photo)"
        >
          <img :src="photo.thumbnailUrl" :alt="photo.fileName" loading="lazy" />
          <div v-if="isSelectMode" class="select-indicator" :class="{ selected: selectedIds.has(photo.id) }">
            <CheckSquare v-if="selectedIds.has(photo.id)" :size="14" />
          </div>
        </div>
      </div>
    </div>
  </section>
</template>
