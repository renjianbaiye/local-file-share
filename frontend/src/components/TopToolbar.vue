<script setup>
import { ref } from 'vue'
import { useRouter } from 'vue-router'
import { Search, Sparkles, LayoutGrid, LayoutList, Columns3, CheckSquare, X, RefreshCw } from '@lucide/vue'
import { usePhotos } from '../composables/usePhotos.js'

defineProps({
  currentView: { type: String, default: 'masonry' },
  showViewSwitch: { type: Boolean, default: true },
})

const emit = defineEmits(['search', 'viewChange'])
const router = useRouter()
const searchQuery = ref('')
const searchRef = ref(null)

const views = [
  { id: 'timeline', icon: LayoutList, label: '时间' },
  { id: 'masonry', icon: Columns3, label: '拼贴' },
  { id: 'grid', icon: LayoutGrid, label: '网格' },
]

const { isSelectMode, toggleSelectMode, refreshPhotos, rebuildTidy, tidyRebuilding } = usePhotos()

const onSearch = () => emit('search', searchQuery.value)
const focusSearch = () => searchRef.value?.focus()
const goTidy = async () => {
  await rebuildTidy()
  router.push('/tidy')
}

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
    </label>

    <div class="toolbar-actions">
      <button class="toolbar-btn icon-btn" @click="refreshPhotos" title="刷新相册">
        <RefreshCw :size="15" />
      </button>

      <button class="toolbar-btn" :class="{ active: isSelectMode }" @click="toggleSelectMode">
        <CheckSquare v-if="!isSelectMode" :size="15" />
        <X v-else :size="15" />
        <span>{{ isSelectMode ? '完成' : '选择' }}</span>
      </button>

      <button class="toolbar-btn primary-btn" :disabled="tidyRebuilding" @click="goTidy">
        <Sparkles :size="15" />
        <span>{{ tidyRebuilding ? '整理中' : '整理' }}</span>
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
