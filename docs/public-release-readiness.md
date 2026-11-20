# Audit avant ouverture publique et publication Fab

Date : 20 novembre 2026. Révision examinée : `66abbadab6eecfe8ab2a77e9899cc5fd70455a05`.

## Décision

Ne pas changer la visibilité immédiatement. Aucun secret réel n'a été identifié par le scan effectué, mais le runner personnel relié aux workflows de pull requests doit être isolé avant l'ouverture. Le dossier Fab doit aussi être régénéré pour la 0.3. Une refonte générale n'est pas nécessaire pour publier.

Aucun changement de visibilité, de licence, de protection de branche ou de configuration du runner n'a été effectué pendant cet audit. Aucun fichier n'a été supprimé et aucun historique Git réécrit.

## Périmètre et limites

- Références locales et références origin récupérées par `git fetch origin` ; 45 commits atteignables, 446 versions uniques de fichiers, environ 3,39 Mo analysés.
- Recherche heuristique locale de clés privées, formes de tokens OpenAI/GitHub/AWS/Slack, secrets affectés et identifiants dans des URL. Les valeurs ne sont pas reproduites dans ce rapport.
- Deux correspondances dans les tests C# : une URL volontairement utilisée à la place d'une clé et une fausse clé explicitement nommée comme telle. Aucun secret réel identifié dans ces correspondances.
- Aucun fichier `.env`, base SQLite, journal, poids GGUF, DLL, EXE ou asset Unreal identifié dans les noms des fichiers Git examinés. Les binaires suivis sont des images de présentation et icônes. La capture source examinée ne montre pas de clé API.
- Les répertoires de travail, paquets, modèles et dépendances générées sont ignorés. Cela ne remplace pas un scan des secrets futur sur chaque contribution.
- L'audit n'inclut pas une preuve d'absence de secrets, un pentest, tous les messages de discussions GitHub, tous les journaux Actions ni une analyse exhaustive des dépendances natives. Un scan heuristique peut manquer des formats inconnus ou des secrets dans des images.

Les preuves détaillées du scan sont locales dans `.gpaa/public-audit/history-scan.json` ; elles ne sont pas destinées à être publiées sans revue.

## Licence

`LICENSE` contient déjà la licence MIT, copyright 2026 Maxime Esprit ; GitHub la reconnaît également comme MIT. Je recommande de la conserver pour le code original : courte, largement réutilisable et adaptée à un projet vitrine. Elle autorise aussi les forks, modifications et usages commerciaux, sous réserve de conserver la notice. Elle n'impose pas la publication des modifications ni un lien promotionnel vers ton profil.

Les composants tiers conservent leur propre licence : ONNX Runtime et llama.cpp sous MIT ; MiniLM et les deux modèles Qwen proposés sous Apache-2.0, d'après les licences et cartes épinglées dans l'inventaire. Les notices transitives doivent rester distribuées. Le code et les composants fournis par Unreal ne deviennent pas MIT du fait de la licence du dépôt.

La licence du dépôt et la licence choisie pour la fiche Fab sont distinctes. Vérifier les conditions applicables aux archives, notamment aux composants Epic et aux Engine Tools : publier des sources originales n'autorise pas automatiquement à republier du code moteur ou tout binaire d'éditeur dans des releases GitHub. Ce rapport ne constitue pas une validation juridique de la distribution.

Références : [MIT / OSI](https://opensource.org/license/mit), [EULA Unreal, sections 5 et 6](https://www.unrealengine.com/eula/unreal), [licences proposées par Fab](https://dev.epicgames.com/documentation/fab/licenses-and-pricing-in-fab).

## Points à régler avant ouverture

### P0 — Runner personnel et contributions publiques

`.github/workflows/ci-unreal.yml` déclenche un job sur un runner Windows auto-hébergé, notamment lors des pull requests. Le filtre excluant les forks est une précaution, mais une condition dans un workflow modifiable par une PR n'est pas une frontière de sécurité suffisante pour exposer une machine personnelle.

Pour cette première publication, le choix le plus simple est de retirer/détacher le runner personnel du dépôt qui deviendra public et de conserver les builds Unreal dans une infrastructure privée ou isolée. Les tests sans Unreal peuvent rester sur les runners hébergés GitHub. Toute réactivation de builds publics sur un runner Unreal devra passer par une conception dédiée : machine éphémère/isolée, absence d'identifiants personnels et exécution uniquement de révisions explicitement approuvées.

Référence : [GitHub — sécurité des runners auto-hébergés](https://docs.github.com/en/actions/reference/security/secure-use).

### P1 — Branche publique et état réel du produit

La branche par défaut est `main`. À la date du contrôle, `origin/main` contient seulement `LICENSE`, `README.md` et `SPEC.md`. Le produit actuel est sur `codex/local-ai`, dans la PR #5 ouverte vers `dev` ; `dev` contient encore la livraison précédente.

Préparer une intégration revue vers la branche destinée au public et faire pointer la page d'accueil vers le vrai produit. Ne pas ouvrir un dépôt dont la branche visible donne l'impression d'un projet presque vide. Vérifier ensuite protections/rulesets et droits d'écriture. L'API de protection de `main` a retourné 403 pour ce dépôt privé et le plan actuel : leur état n'a donc pas été certifié par cet audit.

### P1 — Dossier et preuves Fab 0.3

`docs/fab/third-party-declaration.md`, `docs/fab-submission-readiness.md` et les médias de présentation reflètent encore la livraison 0.2. La capture source montre notamment l'ancien bouton History. Il faut régénérer le dossier pour la révision figée 0.3 : dépendances llama.cpp/Qwen, comportement Local/Cloud, UI actuelle, versions d'Unreal réellement testées et sommes de contrôle des archives exactes.

La dernière CI Unreal consultée passe la construction et les tests mais échoue aux étapes de téléversement des artefacts ; son statut global reste rouge. Aucun artefact Actions n'était listé au moment du contrôle. Résoudre cette publication d'artefacts ou documenter une autre livraison reproductible avant de présenter la CI comme entièrement verte.

### P2 — Hygiène du dépôt public

Compléter les consignes de contribution et de signalement privé des vulnérabilités ; documenter les données locales, les limites du modèle et le rôle des dépendances. Ajouter une protection continue contre les secrets et des règles d'exclusion pour les futurs fichiers locaux sensibles. Vérifier les métadonnées publiques des commits et les chemins personnels conservés dans les anciens comptes rendus. La licence et le nom d'auteur sont déjà présents.

## Qualité de code : bilan ciblé

Points solides : trois modules natifs séparant le cœur, l'inférence et l'intégration éditeur ; ports explicites dans le backend C# de référence ; téléchargements épinglés et vérifiés ; clés hors des fichiers projet ; contrôle des annulations et du shutdown ; tests de stockage, réseau, recherche et de géométrie Slate. Ces éléments sont de bonnes preuves d'ingénierie pour un portfolio.

| Priorité | Constat vérifié | Amélioration proportionnée |
|---|---|---|
| Haute | `SGPAAssistantWidget.cpp` compte environ 905 lignes et mélange construction Slate, fil de discussion, Quick Start, orchestration et réglages. | Extraire le composeur/fil, la vue de recherche et la présentation de Quick Start en petits widgets ; garder un contrôleur de requête clair. |
| Haute | Le widget conserve des handlers, combos et constantes liés à l'ancienne configuration de chat, dont Ollama/Echo, alors que la configuration active est dans les Settings. | Vérifier leurs usages puis supprimer le chemin obsolète et les états dupliqués. Ne pas exposer des providers réellement non supportés. |
| Moyenne | `FGPAANativeRuntime::Run` contient scanning, stockage, sélection de provider et orchestration asynchrone, avec des implémentations concrètes. | Extraire le scan du projet et la préparation du contexte ; introduire des ports uniquement là où ils facilitent réellement les tests et les changements. |
| Moyenne | Les valeurs par défaut sont répétées entre constructeur Settings, reset et Quick Start. | Centraliser les valeurs et le choix du modèle léger, sans effacer les différences intentionnelles du parcours Quick Start. |
| Moyenne | Les étapes et états UI reposent sur des entiers et plusieurs booléens ; des fonctions denses réduisent la lisibilité. | Introduire de petits enums d'étape/état, uniformiser le formatage et expliciter les transitions importantes. |
| Moyenne | README et documentation mélangent produit natif actuel, backend C# de référence et vision AAA future. | Mettre le produit livré en premier, préciser le statut du backend historique et ajouter une carte d'architecture concise. |

SOLID : améliorer les frontières et responsabilités, sans créer une interface pour chaque classe. KISS : retirer les restes avant d'ajouter des couches. DRY : centraliser les règles partagées, sans généraliser des comportements différents. DDD : garder un vocabulaire cohérent (projet, document, passage, source, requête) ; aucun besoin établi d'ajouter CQRS, bus d'événements ou agrégats artificiels à ce plugin.

## Plan borné pour publier

1. **Sécurisation et préparation publique** : isoler la CI Unreal, vérifier la branche publique et les métadonnées, conserver MIT, compléter notices et consignes. Aucun changement fonctionnel.
2. **Nettoyage vitrine limité** : retirer le code natif obsolète, extraire les widgets les plus chargés, centraliser les valeurs communes et améliorer la lisibilité. Garder les tests de comportement, sans modifier les fonctionnalités validées. Éviter une refonte globale du backend C# de référence.
3. **Gel et livraison** : revue finale, intégration des PR, révision/tag figés, reconstruction des archives pour chaque moteur annoncé, tests sur ces archives, médias/documents Fab 0.3 actualisés, puis changement de visibilité et soumission dans des étapes explicites.

La mise en public peut être préparée sans attendre une architecture parfaite. Le critère d'arrêt doit être : risques de publication traités, chemins obsolètes retirés, responsabilités principales lisibles et version reproductible testée — pas un score abstrait de conformité à des acronymes.
