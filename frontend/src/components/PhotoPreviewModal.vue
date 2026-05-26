<script setup>
import { computed, watch } from 'vue'
import { X, Heart, ArrowDownToLine, Trash2, ChevronLeft, ChevronRight } from '@lucide/vue'
import { useKeyboard } from '../composables/useKeyboard.js'
import { usePhotos } from '../composables/usePhotos.js'

const props = defineProps({
  item: { type: Object, default: null },
  items: { type: Array, default: () => [] },
})
const emit = defineEmits(['close', 'navigate'])

const { getTagLabel, formatDay, formatBytes } = usePhotos()

const currentIndex = computed(() => {
  if (!props.item || !props.items.length) return -1
  return props.items.findIndex(i => i.id === props.item.id)
})

const hasPrev = computed(() => currentIndex.value > 0)
const hasNext = computed(() => currentIndex.value < props.items.length - 1)

const goPrev = () => { if (hasPrev.value) emit('navigate', props.items[currentIndex.value - 1]) }
const goNext = () => { if (hasNext.value) emit('navigate', props.items[currentIndex.value + 1]) }

const topTags = computed(() => {
  if (!props.item?.tags) return []
  return props.item.tags
    .filter(t => t.predicted || t.derived)
    .sort((a, b) => (b.probability || 0) - (a.probability || 0))
    .slice(0, 6)
})

const formatDateTime = (seconds) => {
  if (!seconds) return '未知时间'
  return new Date(seconds * 1000).toLocaleString('zh-CN')
}

useKeyboard({
  onEscape: () => emit('close'),
  onPrev: goPrev,
  onNext: goNext,
})

// Lock body scroll when modal is open
watch(() => props.item, (val) => {
  document.body.style.overflow = val ? 'hidden' : ''
}, { immediate: true })
</script>

<template>
  <Transition name="preview-modal">
    <div v-if="item" class="preview-backdrop" @click.self="$emit('close')">
      <article class="preview-container">
        <button class="preview-close" @click="$emit('close')">
          <X :size="18" />
        </button>

        <button v-if="hasPrev" class="preview-nav prev" @click="goPrev">
          <ChevronLeft :size="24" />
        </button>
        <button v-if="hasNext" class="preview-nav next" @click="goNext">
          <ChevronRight :size="24" />
        </button>

        <div class="preview-media">
          <img
            v-if="item.mediaType === 'image'"
            :src="item.downloadUrl || item.thumbnailUrl"
            :alt="item.fileName"
          />
          <video v-else-if="item.mediaType === 'video'" :src="item.downloadUrl" controls />
          <div v-else class="raw-notice">RAW 文件暂不支持预览</div>
        </div>

        <div class="preview-info-panel">
          <h2>{{ item.fileName }}</h2>
          <p class="preview-path">{{ item.folderPath || '/' }}</p>

          <dl class="preview-meta">
            <div><dt>时间</dt><dd>{{ formatDateTime(item.capturedAt || item.modifiedAt) }}</dd></div>
            <div><dt>大小</dt><dd>{{ formatBytes(item.sizeBytes) }}</dd></div>
            <div v-if="item.width"><dt>分辨率</dt><dd>{{ item.width }}×{{ item.height }}</dd></div>
            <div v-if="item.qualityScore"><dt>质量分</dt><dd>{{ (item.qualityScore * 100).toFixed(0) }}%</dd></div>
          </dl>

          <div v-if="topTags.length" class="preview-tags">
            <span
              v-for="tag in topTags"
              :key="tag.tag"
              class="tag-pill"
              :class="{ derived: tag.derived }"
            >
              {{ getTagLabel(tag.tag) }}
            </span>
          </div>

          <div class="preview-actions">
            <button class="action-btn danger" @click="$emit('delete', item)">
              <Trash2 :size="15" />
              删除
            </button>
            <a v-if="item.downloadUrl" class="action-btn primary" :href="item.downloadUrl" download>
              <ArrowDownToLine :size="15" />
              下载
            </a>
            <button class="action-btn" @click="$emit('favorite', item)">
              <Heart :size="15" :fill="item.isFavorite ? 'currentColor' : 'none'" />
              {{ item.isFavorite ? '已收藏' : '收藏' }}
            </button>
          </div>
        </div>
      </article>
    </div>
  </Transition>
</template>
