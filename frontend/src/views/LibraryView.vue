<script setup>
import { ref, computed, onMounted } from 'vue'
import { usePhotos } from '../composables/usePhotos.js'
import { useKeyboard } from '../composables/useKeyboard.js'
import TopToolbar from '../components/TopToolbar.vue'
import PhotoMasonry from '../components/PhotoMasonry.vue'
import TimelineSection from '../components/TimelineSection.vue'
import PhotoPreviewModal from '../components/PhotoPreviewModal.vue'
import EmptyState from '../components/EmptyState.vue'
import LoadingSkeleton from '../components/LoadingSkeleton.vue'
import { Trash2, CheckSquare } from '@lucide/vue'

const {
  photos, photosByDate, favoritePhotos, load, loaded,
  isSelectMode, selectedIds, toggleSelection,
} = usePhotos()

const currentView = ref('masonry')
const previewItem = ref(null)
const searchQuery = ref('')
const toolbarRef = ref(null)

onMounted(() => load())

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
      ),
    }))
    .filter(g => g.items.length > 0)
})

const onSearch = (q) => { searchQuery.value = q }
const openPreview = (photo) => { previewItem.value = photo }
const closePreview = () => { previewItem.value = null }

const deleteSelected = async () => {
  alert(`已选择 ${selectedIds.value.size} 张照片`)
  isSelectMode.value = false
}

useKeyboard({
  onSearch: () => toolbarRef.value?.focusSearch(),
  onEscape: () => { if (previewItem.value) closePreview() },
})
</script>

<template>
  <div class="view-page library-view">
    <div class="library-chrome">
      <header class="library-header">
        <div>
          <h1>图库</h1>
          <p>
            今天整理 {{ photos.length.toLocaleString() }} 张照片
            <span v-if="favoritePhotos.length"> · {{ favoritePhotos.length }} 张收藏</span>
          </p>
        </div>

        <TopToolbar
          ref="toolbarRef"
          :currentView="currentView"
          @viewChange="currentView = $event"
          @search="onSearch"
        />
      </header>

      <LoadingSkeleton v-if="!loaded" />

      <template v-else-if="photos.length === 0">
        <EmptyState
          title="还没有照片"
          description="通过文件传输上传照片，或启动后端扫描索引后，这里会显示你的图库。"
        />
      </template>

      <template v-else>
        <template v-if="currentView === 'timeline'">
          <TimelineSection
            v-for="group in filteredByDate"
            :key="group.date"
            :date="group.date"
            :photos="group.items"
            @preview="openPreview"
          />
        </template>

        <template v-else-if="currentView === 'masonry'">
          <PhotoMasonry :photos="filteredPhotos" @preview="openPreview" />
        </template>

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
    </div>

    <PhotoPreviewModal
      :item="previewItem"
      :items="filteredPhotos"
      @close="closePreview"
      @navigate="previewItem = $event"
    />

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
