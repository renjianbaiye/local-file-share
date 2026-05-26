<script setup>
import { computed } from 'vue'
import { Check, Trash2, Shield } from '@lucide/vue'
import { usePhotos } from '../composables/usePhotos.js'
import { useKeyboard } from '../composables/useKeyboard.js'

const props = defineProps({
  group: { type: Object, required: true },
})
const emit = defineEmits(['close'])

const { markDeleteCandidate, markKeep, isDeleteCandidate, isKeepMarked, formatBytes } = usePhotos()

const bestPhoto = computed(() => props.group.photos?.find(p => p.is_best) || props.group.photos?.[0])
const others = computed(() => props.group.photos?.filter(p => !p.is_best) || [])

useKeyboard({
  onEscape: () => emit('close'),
  onKeep: () => { if (bestPhoto.value) markKeep(bestPhoto.value.id) },
  onDelete: () => {
    // Mark first unmarked other as delete candidate
    const target = others.value.find(p => !isDeleteCandidate(p.id))
    if (target) markDeleteCandidate(target.id)
  },
})
</script>

<template>
  <div class="selection-panel">
    <div class="selection-header">
      <h3>{{ group.title }}</h3>
      <button class="panel-close" @click="$emit('close')">
        返回列表
      </button>
    </div>

    <div class="selection-body">
      <!-- Left: recommended keep -->
      <div class="selection-keep">
        <div class="section-label"><Shield :size="14" /> 推荐保留</div>
        <div class="selection-photo-card recommended" :class="{ kept: isKeepMarked(bestPhoto.id) }">
          <img :src="bestPhoto.thumbnailUrl" :alt="bestPhoto.fileName" />
          <div class="photo-card-meta">
            <span>{{ bestPhoto.fileName }}</span>
            <span>质量 {{ (bestPhoto.quality_score * 100).toFixed(0) }}%</span>
          </div>
          <button class="mark-btn keep" @click="markKeep(bestPhoto.id)">
            <Check :size="14" /> 保留 (K)
          </button>
        </div>
      </div>

      <!-- Right: similar photos -->
      <div class="selection-compare">
        <div class="section-label"><Trash2 :size="14" /> 相似照片对比</div>
        <div class="compare-grid">
          <div
            v-for="photo in others"
            :key="photo.id"
            class="selection-photo-card"
            :class="{ 'delete-marked': isDeleteCandidate(photo.id), kept: isKeepMarked(photo.id) }"
          >
            <img :src="photo.thumbnailUrl" :alt="photo.fileName" />
            <div class="photo-card-meta">
              <span>{{ photo.fileName }}</span>
              <span>质量 {{ (photo.quality_score * 100).toFixed(0) }}%</span>
              <span>相似度 {{ (photo.similarity * 100).toFixed(0) }}%</span>
            </div>
            <div class="photo-card-detail">
              <small>清晰度 {{ (photo.sharpness * 100).toFixed(0) }}%</small>
              <small>曝光 {{ (photo.exposure * 100).toFixed(0) }}%</small>
              <small>{{ photo.resolution }}</small>
              <small>{{ formatBytes(photo.sizeBytes) }}</small>
            </div>
            <div class="photo-card-actions">
              <button class="mark-btn keep" @click="markKeep(photo.id)" :disabled="isKeepMarked(photo.id)">
                <Check :size="12" /> K
              </button>
              <button class="mark-btn delete" @click="markDeleteCandidate(photo.id)" :disabled="isDeleteCandidate(photo.id)">
                <Trash2 :size="12" /> D
              </button>
            </div>
          </div>
        </div>
      </div>
    </div>

    <div class="selection-footer">
      <span class="footer-note">最终由用户确认删除，不会自动删除任何照片</span>
    </div>
  </div>
</template>
