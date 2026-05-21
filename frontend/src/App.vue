<script setup>
import { computed, onMounted, ref } from 'vue'
import {
  ArrowDownToLine,
  ChevronRight,
  Copy,
  FileArchive,
  FileImage,
  FileText,
  Film,
  Folder,
  Heart,
  Image,
  Layers,
  LoaderCircle,
  QrCode,
  RefreshCw,
  Search,
  Sparkles,
  Upload,
  Wifi,
  X,
} from '@lucide/vue'

const activeView = ref('files')

const currentPath = ref('')
const parentPath = ref(null)
const accessUrl = ref('')
const entries = ref([])
const fileStats = ref({ folders: 0, files: 0, totalSize: '0 B' })
const fileLoading = ref(false)
const isUploading = ref(false)
const fileError = ref('')
const fileQuery = ref('')

const albumItems = ref([])
const folders = ref([])
const scanStatus = ref({ status: 'idle' })
const albumLoading = ref(false)
const scanLoading = ref(false)
const albumError = ref('')
const albumQuery = ref('')
const selectedFolder = ref('')
const selectedMedia = ref('all')
const favoriteOnly = ref(false)
const nextCursor = ref(null)
const previewItem = ref(null)
let scanPollTimer = null

const visibleEntries = computed(() => {
  const keyword = fileQuery.value.trim().toLowerCase()
  if (!keyword) return entries.value
  return entries.value.filter((entry) => entry.name.toLowerCase().includes(keyword))
})

const filteredAlbumItems = computed(() => {
  const keyword = albumQuery.value.trim().toLowerCase()
  if (!keyword) return albumItems.value
  return albumItems.value.filter((item) => {
    return item.fileName.toLowerCase().includes(keyword) ||
      item.folderPath.toLowerCase().includes(keyword) ||
      visibleTags(item).some((tag) => tag.tag.toLowerCase().includes(keyword))
  })
})

const groupedAlbumItems = computed(() => {
  const groups = []
  const byDate = new Map()
  for (const item of filteredAlbumItems.value) {
    const key = formatDay(item.capturedAt || item.modifiedAt)
    if (!byDate.has(key)) {
      byDate.set(key, [])
      groups.push({ key, items: byDate.get(key) })
    }
    byDate.get(key).push(item)
  }
  return groups
})

const albumCounts = computed(() => {
  const imageCount = folders.value.reduce((sum, folder) => sum + (folder.photoCount || 0), 0)
  const videoCount = folders.value.reduce((sum, folder) => sum + (folder.videoCount || 0), 0)
  const rawCount = folders.value.reduce((sum, folder) => sum + (folder.rawCount || 0), 0)
  return { imageCount, videoCount, rawCount }
})

const indexedTotal = computed(() => {
  return albumCounts.value.imageCount + albumCounts.value.videoCount + albumCounts.value.rawCount
})

const apiPath = (path = '') => `/api/list${path ? `?path=${encodeURIComponent(path)}` : ''}`

const authenticateFromUrl = async () => {
  const url = new URL(window.location.href)
  const urlToken = url.searchParams.get('token')
  const token = urlToken || import.meta.env.VITE_LFS_TOKEN
  if (!token) return

  const response = await fetch(`/api/auth?token=${encodeURIComponent(token)}`)
  if (!response.ok) throw new Error(`HTTP ${response.status}`)

  if (urlToken) {
    url.searchParams.delete('token')
    window.history.replaceState({}, document.title, `${url.pathname}${url.search}${url.hash}`)
  }
}

const loadDirectory = async (path = '') => {
  fileLoading.value = true
  fileError.value = ''

  try {
    const response = await fetch(apiPath(path))
    if (!response.ok) throw new Error(`HTTP ${response.status}`)
    const data = await response.json()
    currentPath.value = data.currentPath || ''
    parentPath.value = data.parentPath ?? null
    accessUrl.value = data.accessUrl || ''
    entries.value = data.entries || []
    fileStats.value = data.stats || { folders: 0, files: 0, totalSize: '0 B' }
  } catch (error) {
    fileError.value = `Cannot read directory: ${error.message}`
  } finally {
    fileLoading.value = false
  }
}

const loadScanStatus = async () => {
  const response = await fetch('/api/photos/scan/status')
  if (!response.ok) throw new Error(`HTTP ${response.status}`)
  scanStatus.value = await response.json()
}

const loadFolders = async () => {
  const response = await fetch('/api/photos/folders')
  if (!response.ok) throw new Error(`HTTP ${response.status}`)
  const data = await response.json()
  folders.value = data.items || []
}

const timelineUrl = (cursor = null) => {
  const params = new URLSearchParams()
  params.set('limit', '80')
  if (cursor) params.set('cursor', cursor)
  if (selectedFolder.value) params.set('folder', selectedFolder.value)
  if (selectedMedia.value !== 'all') params.set('media', selectedMedia.value)
  if (favoriteOnly.value) params.set('favorite', '1')
  return `/api/photos/timeline?${params.toString()}`
}

const loadTimeline = async ({ append = false } = {}) => {
  albumLoading.value = true
  albumError.value = ''

  try {
    const response = await fetch(timelineUrl(append ? nextCursor.value : null))
    if (!response.ok) throw new Error(`HTTP ${response.status}`)
    const data = await response.json()
    albumItems.value = append ? albumItems.value.concat(data.items || []) : data.items || []
    nextCursor.value = data.nextCursor || null
  } catch (error) {
    albumError.value = `Cannot read album timeline: ${error.message}`
  } finally {
    albumLoading.value = false
  }
}

const reloadAlbum = async () => {
  await Promise.all([loadScanStatus(), loadFolders(), loadTimeline()])
}

const pollScanStatus = () => {
  if (scanPollTimer) window.clearInterval(scanPollTimer)
  scanPollTimer = window.setInterval(async () => {
    try {
      await loadScanStatus()
      if (scanStatus.value.status !== 'scanning') {
        window.clearInterval(scanPollTimer)
        scanPollTimer = null
        await reloadAlbum()
      }
    } catch {
      window.clearInterval(scanPollTimer)
      scanPollTimer = null
    }
  }, 1000)
}

const startScan = async () => {
  scanLoading.value = true
  albumError.value = ''

  try {
    const response = await fetch('/api/photos/scan', { method: 'POST' })
    if (!response.ok) throw new Error(`HTTP ${response.status}`)
    scanStatus.value = await response.json()
    if (scanStatus.value.status === 'scanning') {
      pollScanStatus()
    } else {
      await reloadAlbum()
    }
  } catch (error) {
    albumError.value = `Cannot start scan: ${error.message}`
  } finally {
    scanLoading.value = false
  }
}

const handleUpload = async (event) => {
  const files = event.target.files
  if (!files || files.length === 0) return

  isUploading.value = true
  fileError.value = ''

  const formData = new FormData()
  for (let i = 0; i < files.length; i += 1) {
    formData.append('file', files[i])
  }

  try {
    const response = await fetch(`/api/upload?path=${encodeURIComponent(currentPath.value)}`, {
      method: 'POST',
      body: formData,
    })

    if (!response.ok) {
      const msg = await response.text()
      throw new Error(msg || `HTTP ${response.status}`)
    }

    await loadDirectory(currentPath.value)
    await reloadAlbum()
  } catch (error) {
    fileError.value = `Upload failed: ${error.message}`
  } finally {
    isUploading.value = false
    event.target.value = ''
  }
}

const openEntry = (entry) => {
  if (entry.type === 'folder') {
    loadDirectory(entry.path)
    return
  }
  window.location.href = entry.url
}

const copyUrl = async () => {
  if (!accessUrl.value) return
  await navigator.clipboard?.writeText(accessUrl.value)
}

const setAlbumFilter = async () => {
  nextCursor.value = null
  await loadTimeline()
}

const toggleFavorite = async (item) => {
  const next = !item.isFavorite
  const response = await fetch(`/api/photos/${item.id}/favorite`, {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({ favorite: next }),
  })
  if (!response.ok) return
  item.isFavorite = next
  await loadFolders()
}

const fileType = (entry) => {
  if (entry.type === 'folder') return 'folder'
  const lower = entry.name.toLowerCase()
  if (/\.(zip|rar|7z|tar|gz)$/.test(lower)) return 'archive'
  if (/\.(png|jpg|jpeg|gif|webp|svg|heic)$/.test(lower)) return 'image'
  return 'document'
}

const fileIcon = (entry) => {
  const type = fileType(entry)
  if (type === 'folder') return Folder
  if (type === 'archive') return FileArchive
  if (type === 'image') return FileImage
  return FileText
}

const mediaIcon = (type) => {
  if (type === 'video') return Film
  if (type === 'raw') return Layers
  return Image
}

const tagPriority = (tag, allTags) => {
  const hasPeoplePhoto = allTags.some((item) => item.tag === 'people_photo' && (item.predicted || item.derived))
  const priority = {
    people_photo: 5,
    portrait: 10,
    food: 12,
    building: 15,
    landmark: 16,
    city: 17,
    mountain: 18,
    beach: 19,
    water: 20,
    sky: 21,
    nature: 22,
    landscape: 23,
    indoor: 24,
    night: 25,
    text_or_screen: 30,
    document: 31,
    screenshot: 32,
    text_image: 33,
    travel_checkin: 40,
    travel_or_scenery: 41,
    group_people: 50,
  }
  if (tag.tag === 'person') return hasPeoplePhoto ? 11 : 60
  return priority[tag.tag] ?? (tag.derived ? 45 : 70)
}

const visibleTags = (item) => {
  const tags = (item.tags || []).filter((tag) => tag.predicted || tag.derived)
  return tags
    .slice()
    .sort((a, b) => {
      const priorityDiff = tagPriority(a, tags) - tagPriority(b, tags)
      if (priorityDiff !== 0) return priorityDiff
      return (b.probability || 0) - (a.probability || 0)
    })
    .slice(0, 5)
}

const formatDay = (seconds) => {
  if (!seconds) return '未知日期'
  const d = new Date(seconds * 1000)
  const now = new Date()
  const isThisYear = d.getFullYear() === now.getFullYear()
  const opts = isThisYear
    ? { month: 'long', day: 'numeric', weekday: 'short' }
    : { year: 'numeric', month: 'long', day: 'numeric' }
  return d.toLocaleDateString('zh-CN', opts)
}

const formatDateTime = (seconds) => {
  if (!seconds) return '未知时间'
  return new Date(seconds * 1000).toLocaleString('zh-CN')
}

const formatBytes = (bytes) => {
  if (!bytes) return '0 B'
  const units = ['B', 'KB', 'MB', 'GB', 'TB']
  let value = bytes
  let index = 0
  while (value >= 1024 && index < units.length - 1) {
    value /= 1024
    index += 1
  }
  return `${value.toFixed(value >= 10 || index === 0 ? 0 : 1)} ${units[index]}`
}

onMounted(async () => {
  try {
    await authenticateFromUrl()
    await Promise.all([loadDirectory(), reloadAlbum()])
    if (scanStatus.value.status === 'scanning') {
      pollScanStatus()
    }
  } catch (error) {
    fileError.value = `Authentication failed: ${error.message}`
    albumError.value = `Authentication failed: ${error.message}`
  }
})
</script>

<template>
  <main class="shell">
    <!-- Navigation Bar -->
    <nav class="topbar">
      <button class="brand" type="button" @click="activeView = 'files'">
        <span class="brand-mark"><Wifi :size="17" /></span>
        <span>LocalFileShare</span>
      </button>

      <div class="view-tabs" role="tablist" aria-label="视图切换">
        <button :class="{ active: activeView === 'files' }" type="button" role="tab" @click="activeView = 'files'">
          文件
        </button>
        <button :class="{ active: activeView === 'album' }" type="button" role="tab" @click="activeView = 'album'">
          相册
        </button>
      </div>

      <div class="top-actions">
        <button class="icon-text" type="button" :disabled="!accessUrl" @click="copyUrl">
          <Copy :size="15" />
          复制链接
        </button>
        <a class="icon-text" href="/qr" target="_blank" rel="noreferrer">
          <QrCode :size="15" />
          二维码
        </a>
      </div>
    </nav>

    <!-- Hero Section -->
    <section class="hero-strip">
      <div>
        <p class="eyebrow">LOCAL MEDIA WORKSPACE</p>
        <h1>在局域网内轻松共享文件，浏览索引相册。</h1>
      </div>
      <div class="hero-stats">
        <article>
          <strong>{{ fileStats.files ?? 0 }}</strong>
          <span>文件数</span>
        </article>
        <article>
          <strong>{{ indexedTotal }}</strong>
          <span>已索引媒体</span>
        </article>
        <article>
          <strong>{{ scanStatus.status === 'scanning' ? '扫描中' : scanStatus.status === 'done' ? '已完成' : '待扫描' }}</strong>
          <span>扫描状态</span>
        </article>
      </div>
    </section>

    <!-- ═══════════ FILES VIEW ═══════════ -->
    <section v-if="activeView === 'files'" class="workspace files-layout">
      <aside class="side-panel">
        <div class="panel-title">共享文件夹</div>
        <p class="muted" style="font-size: 13px; margin-bottom: 0;">当前路径: /{{ currentPath }}</p>
        <div class="side-stats">
          <span>{{ fileStats.folders ?? 0 }} 个文件夹</span>
          <span>{{ fileStats.files ?? 0 }} 个文件</span>
          <span>共 {{ fileStats.totalSize ?? '0 B' }}</span>
        </div>
        <label class="upload-button">
          <Upload :size="16" />
          {{ isUploading ? '上传中...' : '上传文件' }}
          <input type="file" multiple :disabled="isUploading" @change="handleUpload" />
        </label>
      </aside>

      <section class="panel">
        <header class="panel-head">
          <label class="search-box">
            <Search :size="16" />
            <input v-model="fileQuery" type="search" placeholder="搜索当前文件夹..." />
          </label>
          <div class="head-actions">
            <button v-if="parentPath !== null" class="ghost-button" type="button" @click="loadDirectory(parentPath)">
              返回上级
            </button>
            <button class="dark-button" type="button" @click="loadDirectory(currentPath)">
              <RefreshCw :size="15" />
              刷新
            </button>
          </div>
        </header>

        <div v-if="fileError" class="notice error">{{ fileError }}</div>
        <div v-else-if="fileLoading" class="notice">正在加载文件列表...</div>
        <div v-else-if="visibleEntries.length === 0" class="notice">当前文件夹为空</div>

        <div v-else class="file-list">
          <article
            v-for="entry in visibleEntries"
            :key="entry.path"
            class="file-row"
            role="button"
            tabindex="0"
            @click="openEntry(entry)"
            @keydown.enter="openEntry(entry)"
          >
            <span class="file-icon" :class="fileType(entry)">
              <component :is="fileIcon(entry)" :size="20" />
            </span>
            <div class="file-main">
              <strong>{{ entry.name }}</strong>
              <span>{{ entry.type === 'folder' ? '文件夹' : '文件' }}</span>
            </div>
            <span class="file-size">{{ entry.size }}</span>
            <span class="row-action">
              <ChevronRight v-if="entry.type === 'folder'" :size="16" />
              <ArrowDownToLine v-else :size="16" />
            </span>
          </article>
        </div>
      </section>
    </section>

    <!-- ═══════════ ALBUM VIEW ═══════════ -->
    <section v-else class="workspace album-layout">
      <aside class="side-panel album-sidebar">
        <button class="scan-button" type="button" :disabled="scanLoading || scanStatus.status === 'scanning'" @click="startScan">
          <LoaderCircle v-if="scanStatus.status === 'scanning'" class="spin" :size="16" />
          <RefreshCw v-else :size="16" />
          {{ scanStatus.status === 'scanning' ? '扫描中...' : '刷新索引' }}
        </button>

        <div class="scan-card">
          <span>扫描状态</span>
          <strong>{{ scanStatus.status === 'scanning' ? '进行中' : scanStatus.status === 'done' ? '已完成' : scanStatus.status || '空闲' }}</strong>
          <small>已发现 {{ scanStatus.totalSeen ?? 0 }}，已移除 {{ scanStatus.totalRemoved ?? 0 }}</small>
        </div>

        <div class="filter-block">
          <label>媒体类型</label>
          <select v-model="selectedMedia" @change="setAlbumFilter">
            <option value="all">全部媒体</option>
            <option value="image">图片</option>
            <option value="video">视频</option>
            <option value="raw">RAW</option>
          </select>
        </div>

        <div class="filter-block">
          <label>文件夹</label>
          <select v-model="selectedFolder" @change="setAlbumFilter">
            <option value="">全部文件夹</option>
            <option v-for="folder in folders" :key="folder.relativePath" :value="folder.relativePath">
              {{ folder.relativePath || '/' }}
            </option>
          </select>
        </div>

        <label class="check-row">
          <input v-model="favoriteOnly" type="checkbox" @change="setAlbumFilter" />
          仅显示收藏
        </label>

        <div class="album-mini-stats">
          <strong>索引统计</strong>
          <span>{{ albumCounts.imageCount }} 张图片</span>
          <span>{{ albumCounts.videoCount }} 个视频</span>
          <span>{{ albumCounts.rawCount }} 个 RAW</span>
          <small>当前网格已加载 {{ albumItems.length }} 项</small>
        </div>
      </aside>

      <section class="panel album-panel">
        <header class="panel-head album-head">
          <label class="search-box wide">
            <Search :size="16" />
            <input v-model="albumQuery" type="search" placeholder="搜索已加载的媒体..." />
          </label>
          <button class="ghost-button" type="button" @click="reloadAlbum">
            <RefreshCw :size="15" />
            重新加载
          </button>
        </header>

        <div v-if="albumError" class="notice error">{{ albumError }}</div>
        <div v-else-if="albumLoading && albumItems.length === 0" class="notice">正在加载相册...</div>
        <div v-else-if="albumItems.length === 0" class="empty-album">
          <Sparkles :size="32" />
          <strong>暂无索引媒体</strong>
          <span>点击「刷新索引」来扫描共享文件夹中的 JPG、PNG、WebP、MP4 和 RAW 文件。</span>
        </div>

        <div v-else class="timeline">
          <section v-for="group in groupedAlbumItems" :key="group.key" class="day-group">
            <h2>{{ group.key }}</h2>
            <div class="album-grid">
              <article v-for="item in group.items" :key="item.id" class="album-card" @click="previewItem = item">
                <div class="thumb">
                  <img
                    v-if="item.mediaType === 'image' && item.thumbnailStatus === 'ready'"
                    :src="item.thumbnailUrl"
                    :alt="item.fileName"
                    loading="lazy"
                  />
                  <component v-else :is="mediaIcon(item.mediaType)" :size="32" />
                  <span class="media-badge">
                    <component :is="mediaIcon(item.mediaType)" :size="12" />
                    {{ item.mediaType === 'image' ? '图片' : item.mediaType === 'video' ? '视频' : 'RAW' }}
                  </span>
                </div>
                <div class="album-card-foot">
                  <strong>{{ item.fileName }}</strong>
                  <button class="favorite-button" :class="{ active: item.isFavorite }" type="button" @click.stop="toggleFavorite(item)">
                    <Heart :size="14" :fill="item.isFavorite ? 'currentColor' : 'none'" />
                  </button>
                </div>
                <div v-if="visibleTags(item).length" class="tag-row">
                  <span v-for="tag in visibleTags(item)" :key="tag.tag" :class="{ derived: tag.derived }">
                    {{ tag.tag }}
                  </span>
                </div>
              </article>
            </div>
          </section>

          <button v-if="nextCursor" class="load-more" type="button" :disabled="albumLoading" @click="loadTimeline({ append: true })">
            {{ albumLoading ? '加载中...' : '加载更多' }}
          </button>
        </div>
      </section>
    </section>

    <!-- ═══════════ PREVIEW MODAL ═══════════ -->
    <div v-if="previewItem" class="preview-backdrop" @click.self="previewItem = null">
      <article class="preview">
        <button class="close-button" type="button" @click="previewItem = null">
          <X :size="18" />
        </button>
        <div class="preview-media">
          <img v-if="previewItem.mediaType === 'image'" :src="previewItem.thumbnailUrl" :alt="previewItem.fileName" />
          <video v-else-if="previewItem.mediaType === 'video'" :src="previewItem.downloadUrl" controls />
          <div v-else class="raw-preview">
            <Layers :size="48" />
            <span>RAW 文件暂不支持预览</span>
          </div>
        </div>
        <div class="preview-info">
          <h2>{{ previewItem.fileName }}</h2>
          <p>{{ previewItem.folderPath || '/' }}</p>
          <dl>
            <div><dt>类型</dt><dd>{{ previewItem.mediaType === 'image' ? '图片' : previewItem.mediaType === 'video' ? '视频' : 'RAW' }}</dd></div>
            <div><dt>大小</dt><dd>{{ formatBytes(previewItem.sizeBytes) }}</dd></div>
            <div><dt>时间</dt><dd>{{ formatDateTime(previewItem.capturedAt || previewItem.modifiedAt) }}</dd></div>
          </dl>
          <div v-if="visibleTags(previewItem).length" class="preview-tags">
            <span v-for="tag in visibleTags(previewItem)" :key="tag.tag" :class="{ derived: tag.derived }">
              {{ tag.tag }}
            </span>
          </div>
          <div class="preview-actions">
            <a class="dark-button" :href="previewItem.downloadUrl">
              <ArrowDownToLine :size="15" />
              下载文件
            </a>
            <button class="ghost-button" type="button" @click="toggleFavorite(previewItem)">
              <Heart :size="15" :fill="previewItem.isFavorite ? 'currentColor' : 'none'" />
              {{ previewItem.isFavorite ? '已收藏' : '收藏' }}
            </button>
          </div>
        </div>
      </article>
    </div>
  </main>
</template>
