/**
 * Speed Milestones - Real speed achievement icons
 * Similar pattern to milestones.js (DRY approach)
 * 
 * Speed is measured in cm/s (calculated client-side from totalTraveled delta)
 * Thresholds are cumulative - highest matching threshold wins
 */

// Speed milestone definitions (sorted by threshold ascending)
// nameKey maps to speedIcons.* i18n keys, resolved at access time
const SPEED_MILESTONES = [
  { threshold: 0,    emoji: '⏸️',  nameKey: 'stopped' },
  { threshold: 0.1,  emoji: '🐌',  nameKey: 'snail' },
  { threshold: 0.5,  emoji: '🐢',  nameKey: 'turtle' },
  { threshold: 2,    emoji: '🚶',  nameKey: 'slowWalk' },
  { threshold: 5,    emoji: '🐕',  nameKey: 'dogTrot' },
  { threshold: 10,   emoji: '🚶‍♂️', nameKey: 'fastWalk' },
  { threshold: 20,   emoji: '🏃',  nameKey: 'jogging' },
  { threshold: 35,   emoji: '🚴',  nameKey: 'bike' },
  { threshold: 50,   emoji: '🐎',  nameKey: 'gallop' },
  { threshold: 70,   emoji: '🏎️',  nameKey: 'maxSpeed' }
];

/**
 * Get speed milestone info for a given speed in cm/s
 * @param {number} speedCmPerSec - Current speed in cm/s
 * @returns {object} { current: {threshold, emoji, name}, next: {threshold, emoji, name}|null }
 */
function getSpeedMilestoneInfo(speedCmPerSec) {
  let current = SPEED_MILESTONES[0]; // Default: stopped
  let next = SPEED_MILESTONES.length > 1 ? SPEED_MILESTONES[1] : null;

  for (let i = SPEED_MILESTONES.length - 1; i >= 0; i--) {
    if (speedCmPerSec >= SPEED_MILESTONES[i].threshold) {
      current = SPEED_MILESTONES[i];
      next = (i + 1 < SPEED_MILESTONES.length) ? SPEED_MILESTONES[i + 1] : null;
      break;
    }
  }

  // Resolve translated names
  const resolve = (m) => m ? { ...m, name: t('speedIcons.' + m.nameKey) } : null;
  return { current: resolve(current), next: resolve(next) };
}
