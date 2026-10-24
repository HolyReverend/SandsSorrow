# Sands of Sorrow

- Unreal Engine 5.8.4. Main implementation language: C++. Discuss changes and write design documentation in Russian; code and asset identifiers in English.
- Before project work read E:/Projects/GameDesign/SandsSorrow/Service/Development.md, E:/Projects/GameDesign/SandsSorrow/TechnicalConcept/Architecture.md, E:/Projects/GameDesign/SandsSorrow/Service/Decisions.md and relevant sections of E:/Projects/GameDesign/SandsSorrow/GameDocuments/Concept.md. Documentation navigation: E:/Projects/GameDesign/SandsSorrow/README.md.
- Before creating, changing or reviewing C++ read and follow E:/Projects/GameDesign/SandsSorrow/Service/CppStandards.md, including its final review and verification checklist. Apply it to new and touched logic; do not refactor unrelated code solely for compliance.
- Keep concept/design/reference material in E:/Projects/GameDesign/SandsSorrow. Keep runtime assets, source, config and development scripts in this project.
- The previous project's structure is a reference, not a requirement. Create folders and abstractions when needed. Preserve the distinction between accepted decisions and proposals.
- Template code lives in Source/SandsOfSorrow/Template; template assets in Content/Template. Do not delete variants or change reflected class names without checking dependent assets.
- Move Unreal assets through Unreal APIs. For World Partition maps use WorldPartitionRenameDuplicateBuilder and verify external actor counts; a successful map load alone is insufficient.
- Save and create a recovery point before bulk asset changes. Check Blueprint compilation status, map loading and PIE after relevant changes.
- Use Scripts/Build-Editor.ps1 for a full editor build with the editor closed. Live Coding does not replace full verification for reflected C++ changes.
- Report test evidence honestly: NullRHI checks do not prove rendering or real keyboard/mouse input.
- After completing work, list every added or modified file in the final chat response with a clickable Markdown link using its absolute path. Distinguish added files from modified files and briefly state what changed. If no files changed, say so.
- При написании и изменении кода оставлять краткие комментарии на русском языке к функциям и отдельным смысловым блокам: пояснять их назначение, что происходит и, при необходимости, неочевидные причины или ограничения. Комментариев должно быть достаточно для общего понимания и навигации как программистом, так и другой моделью, без построчного пересказа, повторения очевидного и избыточного описания. Поддерживать комментарии в соответствии с кодом; имена программных идентификаторов оставлять на английском.

- For the selected UE skills and diagnostic/package/profiling workflows, follow E:/Projects/GameDesign/SandsSorrow/Service/DevelopmentPipeline.md; project context is E:/Projects/GameDesign/SandsSorrow/Service/SkillProjectContext.md. Project rules take precedence over generic skill examples.
