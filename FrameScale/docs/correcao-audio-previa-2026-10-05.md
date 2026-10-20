# Reprodução contínua do áudio na prévia — 05/10/2026

## Caso comunicado

O utilizador comunicou voz entrecortada na reprodução normal, com exportação audível normalmente. O ficheiro de diagnóstico autorizado foi `C:/Users/Admin/Downloads/a.mp4`: vídeo HEVC 1080×1920/60 fps e áudio HE-AAC v2/44,1 kHz. Não foi alterado.

A reprodução pelo QMediaPlayer apresentou avanço da posição superior ao tempo real, também num leitor isolado. A tentativa de criar um vídeo com áudio alongado foi rejeitada pelo utilizador por tornar a voz lenta. Essa abordagem não integra o programa.

## Alteração no programa

A prévia identifica o caso restrito de HE-AAC v2 cuja duração declarada excede significativamente a duração estimada das amostras. Para vídeos de reprodução direta, usa QAudioDecoder e QAudioSink para emitir as amostras continuamente, à frequência nativa de reprodução, deixando o relógio do vídeo independente das marcações esparsas dos pacotes de áudio. Não aplica atempo, não altera o tom e não gera outro vídeo.

O buffer é limitado a um bloco descodificado e ao buffer do dispositivo (200 ms solicitados). Pausa suspende a saída; procura reinicia a leitura e descarta amostras até à posição pedida; fechar ou trocar o conteúdo destrói a saída. Volume e mute seguem os controlos existentes. Os restantes formatos e as exportações mantêm o caminho anterior.

A navegação deste caminho usa o tempo das amostras contínuas, não os intervalos esparsos dos pacotes. No ficheiro fornecido, a descodificação contém cerca de 19,2 segundos de amostras nativas para 38,3 segundos declarados. Não se inventa áudio para preencher essa diferença nem se abranda a voz. Por isso, esta correção não constitui garantia de sincronização com a intenção editorial do ficheiro; essa correspondência requer confirmação auditiva do utilizador. O caminho de compatibilidade não é aplicado aos proxies segmentados.

## Validação

O modo optativo `FrameScaleUiSmoke --audio-diagnostic`, com `FRAMESCALE_AUDIO_SOURCE` apontado ao original, verificou:

- 12 segundos de reprodução real e posição do vídeo de 12000 ms;
- saída de áudio entre 11 e 13 segundos processados nesse intervalo;
- procura para 3 segundos e retoma;
- volume de 72% na saída real e suspensão ao pausar.

A suite nativa geral de interface também passou. Registos em `tmp/finalization-2026-10-05/audio-inapp-run.log` e `audio-fix-ui.log`, relativos à raiz do workspace. A avaliação auditiva de que a voz corresponde ao ritmo esperado continua a depender da confirmação do utilizador.
