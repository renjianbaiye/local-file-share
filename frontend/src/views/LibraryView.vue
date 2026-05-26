<script setup>
import { ref, computed, onMounted } from 'vue'
import { usePhotos } from '../composables/usePhotos.js'
import { useKeyboard } from '../composables/useKeyboard.js'
import TopToolbar from '../components/TopToolbar.vue'
import PhotoMasonry from '../components/PhotoMasonry.vue'
import TimelineSection from '../components/TimelineSection.vue'
import PhotoPreviewModal from '../components/PhotoPreviewModal.vue'
import PhotoRiver from '../components/effects/PhotoRiver.vue'
import EmptyState from '../components/EmptyState.vue'
import LoadingSkeleton from '../components/LoadingSkeleton.vue'
import { Trash2, CheckSquare } from '@lucide/vue'

const { 
  photos, photosByDate, favoritePhotos, load, loaded,
  isSelectMode, selectedIds 
} = usePhotos()

const currentView = ref('masonry')
const previewItem = ref(null)
const searchQuery = ref('')
const toolbarRef = ref(null)

onMounted(() => load())

const riverPhotos = computed(() => {
  return favoritePhotos.value.slice(0, 12).map(p => p.thumbnailUrl)
})

const filteredPhotos = computed(() => {
  if (!searchQuery.value) return photos.value
  const q = searchQuery.value.toLowerCase()
  return photos.value.filter(p =>
    p.fileName?.toLowerCase().includes(q) ||
    p.tags?.some(t => t.tag.includes(q))
  )
})

const filteredByDate = computed(() => {
  if (!searchQuery.value) return photosByDate.value
  const q = searchQuery.value.toLowerCase()
  return photosByDate.value
    .map(g => ({
      ...g,
      items: g.items.filter(p =>
        p.fileName?.toLowerCase().includes(q) ||
        p.tags?.some(t => t.tag.includes(q))
      )
    }))
    .filter(g => g.items.length > 0)
})

const onSearch = (q) => { searchQuery.value = q }
const openPreview = (photo) => { previewItem.value = photo }
const closePreview = () => { previewItem.value = null }

const deleteSelected = async () => {
  console.log('Deleting selected IDs:', Array.from(selectedIds.value))
  alert(`删除了 ${selectedIds.value.size} 张照片`)
  isSelectMode.value = false
}

useKeyboard({
  onSearch: () => toolbarRef.value?.focusSearch(),
  onEscape: () => { if (previewItem.value) closePreview() },
})
</script>

<template>
  <div class="view-page library-view">
    <TopToolbar
      ref="toolbarRef"
      :currentView="currentView"
      @viewChange="currentView = $event"
      @search="onSearch"
    />

    <LoadingSkeleton v-if="!loaded" />

    <template v-else-if="photos.length === 0">
      <EmptyState
        title="还没有照片"
        description="请先通过文件传输上传照片，或启动后端扫描索引。"
      />
    </template>

    <template v-else>
      <!-- Featured Memories River -->
      <section v-if="riverPhotos.length >= 3 && !searchQuery" class="memories-card">
        <div class="memories-header">
          <h2>精选回忆</h2>
          <span>{{ favoritePhotos.length }} 张收藏</span>
        </div>
        <PhotoRiver :photos="riverPhotos" :height="180" :speed="25" direction="left" />
      </section>

      <!-- Timeline View -->
      <template v-if="currentView === 'timeline'">
        <TimelineSection
          v-for="group in filteredByDate"
          :key="group.date"
          :date="group.date"
          :photos="group.items"
          @preview="openPreview"
        />
      </template>

      <!-- Masonry View -->
      <template v-else-if="currentView === 'masonry'">
        <PhotoMasonry :photos="filteredPhotos" @preview="openPreview" />
      </template>

      <!-- Grid View -->
      <template v-else>
        <div class="photo-grid-view">
          <div
            v-for="photo in filteredPhotos"
            :key="photo.id"
            class="grid-cell"
            :class="{ 'is-selected': selectedIds.has(photo.id) }"
            @click="isSelectMode ? toggleSelection(photo.id) : openPreview(photo)"
          >
            <img :src="photo.thumbnailUrl" :alt="photo.fileName" loading="lazy" />
            <div v-if="isSelectMode" class="select-indicator" :class="{ selected: selectedIds.has(photo.id) }">
              <CheckSquare v-if="selectedIds.has(photo.id)" :size="14" />
            </div>
          </div>
        </div>
      </template>
    </template>

    <PhotoPreviewModal
      :item="previewItem"
      :items="filteredPhotos"
      @close="closePreview"
      @navigate="previewItem = $event"
    />

    <!-- Bottom Action Bar for Batch Selection -->
    <Transition name="slide-up">
      <div v-if="isSelectMode" class="bottom-action-bar">
        <div class="selection-info">
          已选择 {{ selectedIds.size }} 项
        </div>
        <div class="actions">
          <button class="action-btn danger" :disabled="selectedIds.size === 0" type="button" @click="deleteSelected">
            <Trash2 :size="16" />
            删除
          </button>
        </div>
      </div>
    </Transition>
  </div>
</template>
