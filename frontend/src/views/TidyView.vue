<script setup>
import { ref, computed, onMounted } from 'vue'
import { Inbox } from '@lucide/vue'
import { usePhotos } from '../composables/usePhotos.js'
import PhotoStackCard from '../components/PhotoStackCard.vue'
import SelectionModePanel from '../components/SelectionModePanel.vue'
import EmptyState from '../components/EmptyState.vue'

const { similarGroups, load, loaded, totalDeleteCandidates, tidyRebuilding, tidyError } = usePhotos()
const expandedGroup = ref(null)

onMounted(() => load())

const totalKeep = computed(() => {
  return similarGroups.value.reduce((sum, g) => sum + g.recommended_keep, 0)
})
const totalCandidates = computed(() => {
  return similarGroups.value.reduce((sum, g) => sum + g.delete_candidates, 0)
})
</script>

<template>
  <div class="view-page tidy-view">
    <header class="view-header">
      <h1>待整理</h1>
      <p>相似照片智能分组，帮你快速筛选最佳照片</p>
    </header>

    <!-- Stats bar -->
    <div v-if="similarGroups.length" class="tidy-stats">
      <div class="stat-item">
        <strong>{{ similarGroups.length }}</strong>
        <span>组相似照片</span>
      </div>
      <div class="stat-item">
        <strong>{{ totalKeep }}</strong>
        <span>推荐保留</span>
      </div>
      <div class="stat-item warn">
        <strong>{{ totalCandidates }}</strong>
        <span>待删除候选</span>
      </div>
      <div v-if="totalDeleteCandidates > 0" class="stat-item marked">
        <strong>{{ totalDeleteCandidates }}</strong>
        <span>已标记删除</span>
      </div>
    </div>

    <EmptyState
      v-if="loaded && similarGroups.length === 0"
      :icon="Inbox"
      title="暂无相似照片分组"
      description="等待后端生成 similar_groups.json 后即可使用智能选图。"
    />

    <!-- Expanded selection panel -->
    <SelectionModePanel
      v-if="expandedGroup"
      :group="expandedGroup"
      @close="expandedGroup = null"
    />

    <!-- Stack cards list -->
    <div v-else class="tidy-stacks">
      <PhotoStackCard
        v-for="group in similarGroups"
        :key="group.id"
        :group="group"
        @expand="expandedGroup = $event"
      />
    </div>
  </div>
</template>
