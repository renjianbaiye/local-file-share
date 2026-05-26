<script setup>
import { ref } from 'vue'
import { useRouter } from 'vue-router'
import { Search, Sparkles, LayoutGrid, LayoutList, Columns3, CheckSquare, X, RefreshCw } from '@lucide/vue'
import { usePhotos } from '../composables/usePhotos.js'

const emit = defineEmits(['search', 'viewChange'])
const props = defineProps({
  currentView: { type: String, default: 'masonry' },
  showViewSwitch: { type: Boolean, default: true },
})

const router = useRouter()
const searchQuery = ref('')
const searchRef = ref(null)

const views = [
  { id: 'timeline', icon: LayoutList, label: '时间流' },
  { id: 'masonry', icon: Columns3, label: '瀑布流' },
  { id: 'grid', icon: LayoutGrid, label: '网格' },
]

const { isSelectMode, toggleSelectMode, scan } = usePhotos()

const onSearch = () => {
  emit('search', searchQuery.value)
}

const focusSearch = () => {
  searchRef.value?.focus()
}

const goTidy = () => router.push('/tidy')

defineExpose({ focusSearch })
</script>

<template>
  <header class="top-toolbar">
    <label class="toolbar-search">
      <Search :size="16" />
      <input
        ref="searchRef"
        v-model="searchQuery"
        type="search"
        placeholder="搜索人物、场景、标签..."
        @input="onSearch"
      />
      <kbd class="search-hint">/</kbd>
    </label>

    <div class="toolbar-actions">
      <button class="toolbar-btn" @click="scan" title="重新扫描并加载最新照片">
        <RefreshCw :size="15" />
        <span>刷新索引</span>
      </button>

      <button class="toolbar-btn" :class="{ 'accent-btn': isSelectMode }" @click="toggleSelectMode">
        <CheckSquare v-if="!isSelectMode" :size="15" />
        <X v-else :size="15" />
        <span>{{ isSelectMode ? '取消选择' : '选择' }}</span>
      </button>

      <button class="toolbar-btn accent-btn" @click="goTidy">
        <Sparkles :size="15" />
        <span>智能整理</span>
      </button>

      <div v-if="showViewSwitch" class="view-switch">
        <button
          v-for="v in views"
          :key="v.id"
          class="view-btn"
          :class="{ active: currentView === v.id }"
          :title="v.label"
          @click="$emit('viewChange', v.id)"
        >
          <component :is="v.icon" :size="16" />
        </button>
      </div>
    </div>
  </header>
</template>
