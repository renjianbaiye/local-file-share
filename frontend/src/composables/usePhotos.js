import { ref, computed } from 'vue'
// Real data composable, removed mock imports

const photos = ref([])
const scenes = ref([])
const persons = ref([])
const similarGroups = ref([])
const deleteCandidates = ref(new Set())
const keepMarked = ref(new Set())
const loaded = ref(false)

const tagLabels = {
  person: '人物', portrait: '人像', landmark: '地标', scenery: '风景',
  city_view: '城市', street: '街景', building: '建筑', architecture: '建筑细节',
  temple_or_historic: '古迹寺庙', sea_or_lake: '海/湖', river_or_water: '河流/水景',
  mountain: '山景', forest: '森林', sky: '天空', beach: '海滩', park: '公园',
  indoor: '室内', restaurant: '餐厅', hotel: '酒店', museum: '博物馆',
  station_or_airport: '车站/机场', vehicle_or_transport: '交通工具', night: '夜景',
  food: '美食', pet: '宠物', document_or_screen: '文档/屏幕', screenshot: '截图',
  people_photo: '人物照片', travel_or_scenery: '旅行/风景', travel_checkin: '旅行打卡',
  text_or_screen: '文字/屏幕', group_people: '多人合照', city: '城市',
  landscape: '风景', nature: '自然', water: '水景', text_image: '文字图片', document: '文档',
}

export function usePhotos() {
  const load = async () => {
    if (loaded.value) return

    // Load real photos from backend API
    try {
      const res = await fetch('/api/photos/timeline?limit=200')
      if (res.ok) {
        const data = await res.json()
        photos.value = data.items || []
      }
    } catch (e) {
      console.error('Failed to load real photos:', e)
      photos.value = [] // Do NOT fallback to mock photos anymore
    }

    // Scene groups
    try {
      const res = await fetch('/api/photos/scene_groups')
      if (res.ok) scenes.value = await res.json()
    } catch { scenes.value = [] }

    // Person groups
    try {
      const res = await fetch('/api/photos/person_groups')
      if (res.ok) persons.value = await res.json()
    } catch { persons.value = [] }

    // Similar groups
    try {
      const res = await fetch('/api/photos/similar_groups')
      if (res.ok) similarGroups.value = await res.json()
    } catch { similarGroups.value = [] }

    loaded.value = true
  }

  const scan = async () => {
    try {
      await fetch('/api/photos/scan', { method: 'POST' })
      // Clear loaded state and reload
      loaded.value = false
      await load()
    } catch (e) {
      console.error('Failed to trigger scan:', e)
      alert('扫描失败，请确保后端服务正常运行')
    }
  }

  const getTagLabel = (tag) => tagLabels[tag] || tag

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

  const photosByDate = computed(() => {
    const groups = new Map()
    for (const photo of photos.value) {
      const key = formatDay(photo.capturedAt || photo.modifiedAt)
      if (!groups.has(key)) groups.set(key, [])
      groups.get(key).push(photo)
    }
    return Array.from(groups.entries()).map(([date, items]) => ({ date, items }))
  })

  const favoritePhotos = computed(() => photos.value.filter(p => p.isFavorite))

  const markDeleteCandidate = (photoId) => {
    deleteCandidates.value.add(photoId)
    keepMarked.value.delete(photoId)
  }

  const markKeep = (photoId) => {
    keepMarked.value.add(photoId)
    deleteCandidates.value.delete(photoId)
  }

  const isDeleteCandidate = (photoId) => deleteCandidates.value.has(photoId)
  const isKeepMarked = (photoId) => keepMarked.value.has(photoId)

  const totalDeleteCandidates = computed(() => deleteCandidates.value.size)

  // Selection Mode State
  const isSelectMode = ref(false)
  const selectedIds = ref(new Set())

  const toggleSelectMode = () => {
    isSelectMode.value = !isSelectMode.value
    if (!isSelectMode.value) selectedIds.value.clear()
  }

  const toggleSelection = (id) => {
    if (selectedIds.value.has(id)) selectedIds.value.delete(id)
    else selectedIds.value.add(id)
  }

  return {
    photos, scenes, persons, similarGroups, loaded,
    deleteCandidates, keepMarked,
    load, getTagLabel, formatDay, formatBytes,
    photosByDate, favoritePhotos,
    markDeleteCandidate, markKeep, isDeleteCandidate, isKeepMarked,
    totalDeleteCandidates,
    isSelectMode, selectedIds, toggleSelectMode, toggleSelection,
    scan,
  }
}
