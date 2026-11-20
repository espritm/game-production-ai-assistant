# Plan d’implémentation — Plugin Unreal autonome

Date : 6 novembre 2026

État : migration engagée ; voir [l’état de livraison et les validations](native-delivery-a-status.md)

Référence fonctionnelle : V1 .NET et plugin validés sur UE 5.6 / Win64, commit `95a2fd8`.

## 1. Objectif produit et périmètre

L’utilisateur installe le plugin depuis Fab, choisit un fournisseur, saisit sa propre clé API, indexe son projet et utilise l’assistant depuis Unreal. Aucune installation de .NET, d’Ollama ou d’une application compagnon n’est nécessaire pour ce parcours cloud.

L’indexation, les embeddings, la recherche et l’historique restent locaux. Seuls la question, l’historique sélectionné et les extraits utiles sont transmis au fournisseur cloud choisi. L’interface explique ce transfert avant la première utilisation du fournisseur.

Deux livraisons successives :

- **A — Plugin autonome avec chat cloud** : recherche locale avec petit modèle d’embeddings embarqué, clé personnelle, réponses progressives, historique et références. C’est le premier candidat Fab.
- **B — Chat entièrement local** : moteur de génération chargé comme bibliothèque native et téléchargement du modèle guidé depuis Unreal. Après installation du modèle, aucun accès réseau n’est nécessaire à ce parcours.

Les fonctions de modification autonome du projet, MCP, génération d’assets, analyse profonde des graphes Blueprint et prise en charge d’autres systèmes d’exploitation restent hors de cette migration.

## 2. Architecture cible et dépendances

Conserver trois modules Unreal de type Editor pour cette première cible :

```text
GPAAssistant ──> GPAACore
GPAAssistant ──> GPAAInference ──> GPAACore
```

| Module | Responsabilités | Dépendances à exclure |
|---|---|---|
| `GPAACore` | Types métier, règles de découpage, cas d’usage, interfaces des fournisseurs et du stockage | Slate, HTTP, Asset Registry, accès direct à SQLite et objets de l’éditeur |
| `GPAAInference` | Adaptateurs cloud, embeddings locaux, puis génération locale et modèles | Interface Slate et orchestration des workflows de l’éditeur |
| `GPAAssistant` | UI, configuration, intégration Unreal, adaptateurs de stockage et de secrets, assemblage des services et cycle de vie | Logique métier dupliquée dans les widgets |

Affinement de l’arborescence proposée : les adaptateurs SQLite et Windows Credential Manager vivent sous `GPAAssistant/Private/Infrastructure`, derrière les interfaces de `GPAACore`. Ainsi le cœur métier ne dépend pas des mécanismes de stockage. Un quatrième module Infrastructure ne sera extrait que si une autre intégration justifie son partage.

Les bibliothèques tierces sont isolées derrière de petits adaptateurs ; leur emplacement, leurs licences et leurs fichiers redistribués doivent suivre les exigences Fab. Le choix définitif du moteur d’embeddings et du modèle intervient après prototype.

Contrats principaux :

- `IGPAAChatProvider` : capacités, génération asynchrone, fragments, fin, erreur et annulation.
- `IGPAAEmbeddingProvider` : identité complète du modèle, encodage de documents et de requêtes, dimension, traitement par lots.
- `IGPAAKnowledgeRepository` : lecture d’un état cohérent, mise à jour atomique de l’index et migrations du schéma.
- `IGPAAChatHistoryRepository` : conversations locales et références utilisées.
- `IGPAASecretStore` : stockage, lecture et suppression d’une clé personnelle, sans exposition aux exports.
- Un contexte de tâche commun : identifiant de requête, progression, annulation et durée limite.

## 3. Stratégie de migration

Introduire temporairement un moteur d’exécution interne sélectionnable : `LegacyBackend` ou `Native`. Le moteur natif progresse derrière les mêmes workflows UI. La distribution actuelle demeure utilisable jusqu’à la validation de la livraison A.

Les services C# servent de référence pour les règles métier et les scénarios. Leurs tests deviennent des jeux de données et assertions portables. On ne compare pas des vecteurs numériques issus de modèles différents ; on compare les comportements déterministes et la qualité des résultats sur un corpus fixé.

Ne pas transformer le serveur .NET en dépendance cachée ni télécharger son exécutable au premier lancement pour réaliser cette migration.

### Correspondance avec le code existant

| Existant | Destination native |
|---|---|
| `SourceChunker`, `SourceContextExtractor`, `VectorMath` | Services de domaine dans `GPAACore` |
| `ProjectIndexingService`, `SemanticSearchService`, `ProjectChatService` | Cas d’usage dans `GPAACore/Application` |
| `LocalProjectFileScanner` et documents Asset Registry | Adaptateurs de projet dans `GPAAssistant/Integration` |
| Référentiels JSON | Adaptateurs SQLite locaux et import éventuel de l’historique |
| `OpenAICompatibleChatCompletionProvider` | Adaptateur OpenAI Responses et adaptateur Chat Completions distincts |
| `GPAAProjectWorkflow`, `GPAAChatSettingsWorkflow` | Façades UI appelant le moteur sélectionné |
| `GPAAHttpClient`, `GPAABackendSupervisor` | Conservés pendant la transition, retirés du package natif final |
| Slate, présentation de la connaissance, ouverture des sources | Réutilisés et adaptés progressivement |

## 4. Lots d’implémentation

### Lot 0 — Lever les risques de distribution et figer les mesures

**Dépendance : aucune.**

- Identifier la dernière version stable d’Unreal requise pour une première soumission Fab. Conserver UE 5.6 comme base de régression et prévoir le runner supplémentaire nécessaire.
- Prototyper l’inférence d’embeddings CPU dans Unreal à partir d’une bibliothèque native et d’un modèle candidat. ONNX Runtime est une option à évaluer, pas un choix déjà validé.
- Vérifier le tokenizer, le prétraitement, le pooling et la normalisation du modèle, ainsi que les éventuels préfixes distincts pour requêtes et documents.
- Vérifier la licence du modèle, de son tokenizer et du runtime, leur redistribution sur Fab, les notices et la déclaration de logiciels tiers.
- Tester le chargement depuis un plugin moteur installé dans un emplacement non modifiable par l’utilisateur.
- Fixer un petit corpus Unreal représentatif, des questions avec sources attendues et un projet moyen de référence. Mesurer précision de recherche, durée d’indexation, mémoire et latence de recherche.
- Reprendre les objectifs de `SPEC.md` — indexation initiale sous cinq minutes et recherche sous cinq secondes — comme cibles à confirmer sur ce corpus et une machine décrite.

**Sortie attendue :** décision documentée sur le couple runtime/modèle, limites matérielles, corpus et seuils d’acceptation. Si le modèle ne peut pas être embarqué, réexaminer l’expérience du premier lancement avant de poursuivre la promesse « clé API puis prêt à utiliser ».

### Lot 1 — Installer les modules et le mécanisme de transition

**Dépendance : lot 0 pour les contraintes structurantes.**

- Créer `GPAACore` et `GPAAInference`, leurs fichiers `Build.cs`, leurs interfaces publiques minimales et leurs tests.
- Déclarer les modules et la plateforme Win64 dans le descripteur.
- Introduire la façade d’exécution et le choix interne `LegacyBackend` / `Native`.
- Assembler les services dans le module éditeur, sans service locator global dans le domaine.
- Définir la propriété des tâches et des requêtes HTTP ; leur annulation et leurs callbacks ne doivent pas survivre à l’arrêt du module ni accéder à des widgets détruits.

**Validation :** compilation des trois modules, chargement et déchargement, absence de dépendance circulaire et parcours historique toujours fonctionnel.

### Lot 2 — Porter l’indexation et le stockage locaux

**Dépendance : lot 1.**

- Porter les types métier, le découpage avec chevauchement, l’extraction de contexte et les règles d’exclusion des fichiers générés.
- Collecter directement les métadonnées Asset Registry, sans aller-retour HTTP ni export JSON intermédiaire obligatoire.
- Copier les données Unreal nécessaires dans des structures indépendantes avant les traitements de fond ; respecter le thread requis par les API moteur.
- Définir une politique stable pour les chemins, l’encodage, les empreintes de contenu et l’identité des projets.
- Implémenter SQLite derrière les référentiels. Prévoir les tables de fichiers, fragments, vecteurs, métadonnées d’assets, conversations et versions de schéma.
- Enregistrer l’identité d’index : version du schéma, modèle et révision, tokenizer, prétraitement, dimension et normalisation des embeddings.
- Effectuer les écritures via un unique écrivain et des transactions. Une annulation ou un crash conserve le dernier index cohérent.
- Stocker les données projet sous `Saved/GPAA` et les modèles partagés dans le dossier utilisateur de GPAA ; ne rien écrire dans le répertoire d’installation du plugin.
- Afficher progression, annulation, fichiers réutilisés et changements détectés. Définir le verrouillage par projet pour les accès concurrents.

**Validation :** mêmes fragments et métadonnées attendus sur les fixtures, ajout/modification/suppression détectés, réutilisation des fichiers inchangés, reprise après interruption et aucune attente longue sur le thread de l’éditeur.

### Lot 3 — Activer la recherche sémantique sans service externe

**Dépendances : lots 0 et 2.**

- Intégrer le runtime et le petit modèle d’embeddings retenus, CPU en première cible pour limiter les dépendances matérielles.
- Implémenter le tokenizer et la pipeline d’encodage validée, le traitement par lots bornés et les limites mémoire.
- Charger le modèle à la demande et réutiliser sa session d’inférence.
- Porter la recherche par similarité, les filtres Code/Assets et les références fichier/lignes. Commencer par un classement exact borné ; décider d’un index vectoriel spécialisé à partir des mesures.
- En cas de changement du modèle ou de sa pipeline, reconstruire l’index. Ne jamais comparer des vecteurs de dimensions ou de provenances incompatibles.
- Rendre une panne d’embeddings explicite ; ne pas afficher silencieusement des résultats lexicaux comme s’ils étaient sémantiques.

**Validation :** indexation et recherche hors connexion avec Ollama et le serveur .NET arrêtés, résultats satisfaisant les seuils du corpus, progression et annulation opérationnelles.

### Lot 4 — Connecter le chat cloud directement depuis Unreal

**Dépendance : lot 1 ; branchement complet après le lot 3.**

- Implémenter un fournisseur OpenAI Responses et un fournisseur compatible Chat Completions séparé, derrière `IGPAAChatProvider`.
- Utiliser le module HTTP Unreal avec génération asynchrone. Parser le flux SSE incrémentalement : événements fragmentés, plusieurs événements par réception et caractères UTF-8 incomplets.
- Distinguer texte reçu, réponse terminée, réponse interrompue, annulation et erreur après réception partielle.
- Gérer authentification refusée, modèle indisponible, quota, débit, déconnexion et durée limite. Borner les nouvelles tentatives pour éviter les réponses dupliquées et les coûts inattendus.
- Décrire les capacités par fournisseur/modèle : paramètres disponibles, streaming et limites de contexte. Ne pas appliquer systématiquement les mêmes paramètres à tous les modèles.
- Implémenter Windows Credential Manager derrière `IGPAASecretStore`. Les paramètres sauvegardés contiennent une référence de secret ; les clés sont exclues du projet, des exports et des journaux.
- Pour les endpoints personnalisés, associer explicitement clé et destination ; éviter tout transfert d’authentification vers un autre hôte lors d’une redirection.
- Garder l’historique applicatif local et fixer explicitement les options de conservation des réponses proposées par le fournisseur ; ne pas promettre une absence totale de conservation côté fournisseur.

**Validation :** tests de contrat avec un faux transport HTTP et des flux enregistrés, incluant les erreurs et l’annulation. Les tests CI n’utilisent aucune clé réelle et n’engagent aucun appel payant. Un test réel reste optionnel et explicite.

### Lot 5 — Réaliser le parcours complet dans l’interface

**Dépendances : lots 2, 3 et 4.**

- Porter le constructeur de contexte : sélection des sources, budget adapté au modèle, références structurées et distinction entre documents et instructions.
- Relier `Ask`, `Search`, `Refresh Knowledge` et l’historique aux services natifs. L’UI reçoit des événements de progression et des résultats typés.
- Ajouter le premier lancement : choix du fournisseur, saisie de la clé, test de connexion, information sur les extraits transmis, puis indexation.
- Pour la livraison A, afficher clairement que le mode totalement local n’est pas encore disponible ; ne pas proposer un bouton sans implémentation.
- Afficher le texte progressivement et garder l’ouverture des sources et les filtres existants.
- Empêcher qu’un callback d’une ancienne question remplace le résultat d’une nouvelle question. Sauvegarder les réponses complètes et identifier les réponses interrompues.
- Réindexer les changements nécessaires avec un état de fraîcheur visible, sans reconstruire inutilement tout le projet à chaque question.

**Validation :** parcours cloud complet après installation du plugin et saisie d’une clé, sans backend, sans .NET et sans Ollama ; pas de régression sur les actions existantes.

### Lot 6 — Basculer la distribution et préparer le candidat Fab

**Dépendances : lots 0 à 5 validés. Livraison A.**

- Rendre le moteur natif par défaut après comparaison des scénarios avec la V1. Retirer du package natif le client HTTP local, le superviseur, les paramètres de backend et les ressources .NET.
- Recréer l’index dans un nouvel espace de stockage. Prévoir un import explicite de l’historique JSON utile ; ne pas convertir silencieusement les anciens vecteurs ni recopier des clés en clair.
- Conserver temporairement le backend C# comme référence de développement ; supprimer ses projets dans un changement ultérieur, après stabilisation du natif.
- Séparer le package précompilé de validation et le ZIP source de soumission Fab. Le ZIP Fab n’inclut aucun EXE/MSI ni les dossiers de build locaux ; il contient les sources et les dépendances autorisées avec leurs notices.
- Ajouter un vérificateur Fab distinct : `EngineVersion`, plateformes des modules, copyright des sources, chemins, fichiers interdits, filtres des dossiers supplémentaires et déclarations tierces. Renseigner `FabURL` lorsque l’identifiant produit existe, sans inventer de valeur.
- Conserver les tests de la distribution historique tant qu’elle est supportée. Ajouter une CI native qui n’installe pas le SDK .NET pour fabriquer le plugin.
- Valider UE 5.6 et la dernière version stable retenue au lot 0. Chaque version annoncée reçoit sa compilation et son package.
- Exécuter un smoke test natif sur le package exact : chargement, embeddings et recherche hors connexion, chat simulé, annulation, fermeture et absence de processus backend. Couvrir aussi l’installation comme plugin moteur dans un environnement de validation dédié.
- Vérifier sur un environnement sans runtime .NET ni Ollama : une machine de développement où ces outils sont installés ne suffit pas à prouver leur absence comme dépendance.
- Préparer la documentation anglaise, la fiche, les captures et la liste honnête des versions/plateformes prises en charge. La publication effective passe par la revue Fab et le profil éditeur.

**Validation :** plugin utilisable sans application compagnon, CI native au vert, ZIP de soumission vérifié, notices et dépendances revues. Ce jalon rend le produit candidat à la revue Fab ; il ne garantit pas son approbation.

### Lot 7 — Ajouter le téléchargement et la génération locale

**Dépendance : livraison A stabilisée. Livraison B.**

- Prototyper un moteur de génération comme bibliothèque native, sans exécutable auxiliaire. Valider redistribution, compatibilité Unreal et qualité avant de retenir un runtime.
- Implémenter `IGPAAChatProvider` pour ce moteur ; réutiliser contexte, streaming, historique et UI.
- Proposer un catalogue limité de modèles testés, avec taille, licence, mémoire nécessaire et qualité attendue. Détecter le matériel et expliquer les limites ; ne pas promettre que tout modèle fonctionne sur toute machine.
- Télécharger depuis des sources connues avec progression, reprise, annulation, contrôle d’espace et vérification d’intégrité. Installer d’abord dans un fichier temporaire, puis activer le modèle une fois complet.
- Ajouter chargement/déchargement, limite de mémoire et sélection CPU/GPU selon les combinaisons réellement validées. Les bibliothèques GPU ne deviennent pas obligatoires pour le mode cloud.
- Ne pas basculer automatiquement vers le cloud en cas de panne locale.
- Réutiliser le petit modèle d’embeddings de la livraison A, indépendamment du modèle de génération choisi.

**Validation :** installation et téléchargement depuis Unreal uniquement, questions et recherches hors connexion après préparation, aucun serveur externe requis, annulation et manque de mémoire traités sans arrêt de l’éditeur dans les cas testés.

## 5. Ordre des changements et suivi

Chaque lot produit un changement relisible et ses preuves de validation. Les lots 2 et 4 sont techniquement indépendants une fois les contrats du lot 1 établis, mais aucune délégation ou exécution parallèle n’est nécessaire pour suivre ce plan.

Ordre recommandé :

```text
0. Prototype et décisions
1. Modules et interfaces
2. Indexation et SQLite
3. Embeddings et recherche
4. Fournisseurs cloud et secrets
5. Parcours utilisateur complet
6. Distribution native et candidat Fab        -> Livraison A
7. Téléchargement et génération locale        -> Livraison B
```

Le plan n’attribue pas de délai artificiellement précis à la migration. Le lot 0 doit donner assez de mesures pour estimer le reste, notamment l’intégration native des modèles et la compatibilité avec la dernière version Unreal.

## 6. Première action concrète

Commencer par le lot 0 : construire une preuve de fonctionnement des embeddings locaux dans un plugin Unreal, mesurer sa recherche sur un corpus de code représentatif et documenter les licences et la version Unreal cible. Ce prototype décide si le parcours cloud pourra réellement fonctionner après une seule installation du plugin et la saisie d’une clé.

## Références

- [Spécification V1](../SPEC.md) et [guide développeur actuel](developer-guide.md).
- [Packaging et CI/CD existants](plugin-packaging-ci-cd.md).
- [Exigences techniques Fab](https://www.fab.com/o/technical-requirements), notamment versions moteur, logiciels tiers, plugins de code et structure des archives. À revérifier avant soumission.
- [Publication sur Fab](https://dev.epicgames.com/documentation/fab/publishing-assets-for-sale-or-free-download-in-fab).
- [Streaming OpenAI Responses](https://developers.openai.com/api/docs/guides/streaming-responses).
