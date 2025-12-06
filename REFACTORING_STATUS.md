# État du Refactoring Frontend - ESP32 Stepper Controller

**Date:** 6 décembre 2025  
**Objectif:** Découpler et modulariser le JavaScript frontend (7300+ lignes)

---

## 📊 État Actuel

### Structure des fichiers JS (`data/js/`)

| Fichier | Lignes | Rôle | État |
|---------|--------|------|------|
| `app.js` | 256 | AppState, constantes, SystemState enum | ✅ Stable |
| `utils.js` | 241 | Utilitaires (formatage, helpers) | ✅ Stable |
| `milestones.js` | ~100 | Données achievements/milestones | ✅ Stable |
| `websocket.js` | 223 | Connexion WS, handlers | ✅ Stable |
| `stats.js` | 290 | Statistiques, graphiques Chart.js | ✅ Stable |
| `context.js` | ~160 | Container DI, fonctions génériques | ✅ Nettoyé |
| `chaos.js` | ~120 | Fonctions pures mode Chaos | ✅ Fonctionnel |
| `oscillation.js` | ~180 | Fonctions pures mode Oscillation | ✅ Fonctionnel |
| `sequencer.js` | ~330 | Fonctions pures séquenceur + tooltips | ✅ Fonctionnel |
| `presets.js` | ~220 | **NOUVEAU** - Preset name/tooltip + decel curves | ✅ Fonctionnel |
| `main.js` | ~6950 | Logique principale (wrappers DOM) | 🔄 En cours |

### Ordre de chargement dans `index.html`
```html
<script src="/js/milestones.js"></script>
<script src="/js/app.js"></script>
<script src="/js/utils.js"></script>
<script src="/js/websocket.js"></script>
<script src="/js/stats.js"></script>
<script src="/js/context.js"></script>     <!-- DI Container -->
<script src="/js/chaos.js"></script>       <!-- Chaos pure functions -->
<script src="/js/oscillation.js"></script> <!-- Oscillation pure functions -->
<script src="/js/sequencer.js"></script>   <!-- Sequencer pure functions -->
<script src="/js/presets.js"></script>     <!-- Preset name/tooltip pure functions -->
<script src="/js/main.js"></script>
```

---

## ✅ Ce qui a été fait

### 1. Extraction main.js (Phase 1)
- `index.html` réduit de 9003 → 1703 lignes (HTML pur)
- Tout le JS inline extrait dans `main.js`

### 2. Création context.js (DI Container)
Container d'injection de dépendances avec fonctions génériques :

```javascript
// Container DI
const Context = { config: null, stepsPerMM: 100 };
initContext()
// Utilitaires génériques
mmToSteps(mm, stepsPerMM)
stepsToMm(steps, stepsPerMM)
getEffectiveMaxDistMM()
getTotalDistMM()
// Wrappers contextuels (utilisent Context)
validateChaosLimitsCtx(), validateOscillationLimitsCtx()
```

### 3. Création chaos.js (Module Chaos - Phase 2)
Module dédié au mode Chaos avec constantes et fonctions pures :

```javascript
// Constantes
CHAOS_LIMITS = { SPEED_MIN, SPEED_MAX, AMPLITUDE_MIN, AMPLITUDE_MAX, ... }
CHAOS_PATTERNS = { RANDOM: 1, SWEEP: 2, BURST: 4, ... }
// Fonctions PURES
validateChaosLimitsPure(centerPos, amplitude, totalDistMM)
buildChaosConfigPure(formValues)
countEnabledPatternsPure(patterns)
generateChaosTooltipPure(config)
```

### 4. Création oscillation.js (Module Oscillation - Phase 2)
Module dédié au mode Oscillation avec constantes et fonctions pures :

```javascript
// Constantes
OSCILLATION_LIMITS = { AMPLITUDE_MIN, AMPLITUDE_MAX, SPEED_MIN, SPEED_MAX, ... }
WAVEFORM_TYPE = { SINE: 0, TRIANGLE: 1, SQUARE: 2 }
// Fonctions PURES
validateOscillationLimitsPure(centerPos, amplitude, totalDistMM)
buildOscillationConfigPure(formValues)
calculateOscillationPeakSpeedPure(amplitude, speed)
generateOscillationTooltipPure(config)
formatCyclePauseInfoPure(cyclesBeforePause, pauseDuration)
```

### 5. Extension sequencer.js (Module Séquenceur - Phase 2)
Module complet avec validation ET génération de tooltips :

```javascript
// Constantes
SEQUENCER_LIMITS = { SPEED_MIN, SPEED_MAX, DECEL_ZONE_MIN, ... }
MOVEMENT_TYPE = { VAET: 0, OSCILLATION: 1, CHAOS: 2, CALIBRATION: 4 }
DECEL_MODES = { NONE: 0, EARLY: 1, VERY_EARLY: 2 }
// Fonctions PURES
validateSequencerLinePure(line, movementType, effectiveMax)
buildSequenceLineDefaultsPure(effectiveMax)
generateSequenceLineTooltipPure(line, movementType, config)
generateVaetTooltipPure(line, effectiveMax)
generateCalibrationTooltipPure(line)
```

### 6. Modification main.js - Pattern de délégation
Les fonctions de main.js délèguent aux fonctions pures avec fallback :

```javascript
// Exemple: generateSequenceLineTooltip() délègue à generateSequenceLineTooltipPure()
function generateSequenceLineTooltip(line, movementType) {
  if (typeof generateSequenceLineTooltipPure === 'function') {
    const config = { ... };  // Récupération du contexte
    return generateSequenceLineTooltipPure(line, movementType, config);
  }
  // Fallback si module non chargé
  return '';
}
```

### 7. Routes serveur (APIRoutes.cpp)
Toutes les routes JS configurées avec cache 24h :
- `/js/app.js`, `/js/utils.js`, `/js/milestones.js`
- `/js/websocket.js`, `/js/stats.js`
- `/js/context.js`, `/js/chaos.js`, `/js/oscillation.js`
- `/js/sequencer.js`, `/js/main.js`

---

## 🎯 Prochaines Étapes (Phase 3 - Extraction DOM)

### Stratégie : Découpler AVANT d'extraire
1. ✅ Créer la fonction pure dans le module cible
2. ✅ Modifier `main.js` pour déléguer à la fonction pure (avec fallback)
3. Tester que tout fonctionne
4. Répéter pour d'autres fonctions

### Module Sequencer - État actuel ✅
Fonctions migrées vers `sequencer.js` :
- [x] `validateSequencerLinePure()` - Validation des lignes
- [x] `buildSequenceLineDefaultsPure()` - Valeurs par défaut
- [x] `generateSequenceLineTooltipPure()` - Génération tooltip ligne
- [x] `generateVaetTooltipPure()` - Tooltip mode VAET
- [x] `generateCalibrationTooltipPure()` - Tooltip mode Calibration

### Module Oscillation - État actuel ✅
Fonctions migrées vers `oscillation.js` :
- [x] `validateOscillationLimitsPure()` - Validation limites
- [x] `buildOscillationConfigPure()` - Construction config
- [x] `calculateOscillationPeakSpeedPure()` - Calcul vitesse de pointe
- [x] `generateOscillationTooltipPure()` - Génération tooltip
- [x] `formatCyclePauseInfoPure()` - Formatage info pause

### Module Chaos - État actuel ✅
Fonctions migrées vers `chaos.js` :
- [x] `validateChaosLimitsPure()` - Validation limites
- [x] `buildChaosConfigPure()` - Construction config
- [x] `countEnabledPatternsPure()` - Comptage patterns actifs
- [x] `generateChaosTooltipPure()` - Génération tooltip

### Module UI - À créer (`ui-helpers.js`) - Phase 3
Fonctions utilitaires UI à extraire :
- [ ] `formatPositionPure(positionMM, currentStep)`
- [ ] `getStateDisplayPure(stateCode, errorMessage)`
- [ ] `formatSpeedPure(speedLevel, maxSpeed)`
- [ ] `updateUIElementPure()` - Helpers DOM génériques

### Module Forms - À créer (`form-handlers.js`) - Phase 3
Gestion des formulaires :
- [ ] `collectFormValuesPure()` - Extraction valeurs formulaire
- [ ] `validateFormPure()` - Validation générique

---

## 🔧 Commandes Utiles

```powershell
# Compiler firmware
C:\Users\Administrator\.platformio\penv\Scripts\platformio.exe run

# Upload firmware
C:\Users\Administrator\.platformio\penv\Scripts\platformio.exe run -t upload

# Upload fichiers web
python upload_html.py --all           # Tous les fichiers
python upload_html.py --file data/js/main.js   # Un fichier spécifique
python upload_html.py --js            # Tous les JS

# Vérifier un fichier
Select-String -Path "data\js\main.js" -Pattern "validateChaos"
```

---

## ⚠️ Notes Importantes

1. **VS Code peut avoir des problèmes** avec la création/édition de fichiers. En cas de doute, utiliser PowerShell directement pour vérifier le contenu réel des fichiers.

2. **Les logs console.log dans context.js** ne s'affichent que si DevTools est ouvert AVANT le chargement de la page.

3. **Tester les fonctions pures** dans la console navigateur :
```javascript
validateChaosLimitsPure(500, 200, 1000)  // {valid: true, ...}
validateChaosLimitsPure(100, 200, 1000)  // {valid: false, error: "..."}
```

4. **Le pattern de découplage** :
   - Fonction pure dans `context.js` (logique sans DOM)
   - Wrapper dans `main.js` qui récupère les valeurs DOM et appelle la fonction pure
   - Permet les tests unitaires et la réutilisation

---

## 📁 Fichiers Clés

- `data/index.html` - HTML pur (1703 lignes)
- `data/js/context.js` - Container DI + utilitaires (~160 lignes)
- `data/js/chaos.js` - Module Chaos pure functions (~120 lignes)
- `data/js/oscillation.js` - Module Oscillation pure functions (~180 lignes)
- `data/js/sequencer.js` - Module Séquenceur pure functions (~330 lignes)
- `data/js/main.js` - Logique principale (~7020 lignes) - À réduire
- `src/web/APIRoutes.cpp` - Routes serveur HTTP
- `upload_html.py` - Script d'upload vers ESP32
