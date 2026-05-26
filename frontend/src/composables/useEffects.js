import { ref, computed, onMounted } from 'vue'

const effectsEnabled = ref(true)
const reducedMotion = ref(false)
const lowPerf = ref(false)

let initialized = false

export function useEffects() {
  if (!initialized) {
    const saved = localStorage.getItem('album-effects')
    if (saved !== null) effectsEnabled.value = saved !== 'false'

    if (typeof window !== 'undefined') {
      const mq = window.matchMedia?.('(prefers-reduced-motion: reduce)')
      reducedMotion.value = mq?.matches ?? false
      mq?.addEventListener?.('change', (e) => { reducedMotion.value = e.matches })

      // Simple heuristic for low-perf devices
      const cores = navigator.hardwareConcurrency || 4
      lowPerf.value = cores <= 2
    }
    initialized = true
  }

  const canAnimate = computed(() => effectsEnabled.value && !reducedMotion.value)

  const effectDensity = computed(() => {
    if (!canAnimate.value) return 0
    if (lowPerf.value) return 0.3
    return 1
  })

  const toggleEffects = () => {
    effectsEnabled.value = !effectsEnabled.value
    localStorage.setItem('album-effects', String(effectsEnabled.value))
  }

  return {
    effectsEnabled, reducedMotion, lowPerf,
    canAnimate, effectDensity, toggleEffects,
  }
}
