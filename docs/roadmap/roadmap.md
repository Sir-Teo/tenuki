Below is a practical, state‑of‑the‑art roadmap to build a Go (weiqi/baduk) engine today. It’s organized in stages, with concrete architecture choices, acceptance tests, and pointers to the best open work so you can make strong engineering choices from day one.

⸻

0) What “state‑of‑the‑art” means in 2025 (targets to aim for)

Modern top engines (notably KataGo) are AlphaZero‑style systems: a residual CNN predicts policy and value and drives a PUCT search; today’s SOTA adds (a) domain‑general efficiency gains, and (b) Go‑specific heads/targets such as ownership and score that make handicap play/endgames and analysis far stronger. KataGo also introduced many search and training refinements and recently documented Monte‑Carlo Graph Search (transpositions) rather than a strict tree.  ￼

Keep these “north‑stars” in mind:
	•	AlphaZero core: policy/value net + MCTS/PUCT with Dirichlet root noise & temperature.  ￼
	•	KataGo efficiency & strength: playout‑cap randomization, policy‑target pruning, global pooling, auxiliary policy, ownership and score heads, plus later methods like dynamic cPUCT, uncertainty‑weighted playouts, soft policy target, etc.  ￼
	•	Graph Search: search on a DAG with transpositions (not just a tree) for stronger reuse.  ￼
	•	Robustness: adversarial failures (e.g., cyclic/pass attacks) exist; plan in defenses & eval.  ￼

⸻

1) Scope & strategy (pick a track)

Track A — “From‑scratch SOTA” (research‑grade).
You build the full self‑play + training + search stack (AlphaZero/KataGo‑class). This is the path if you want to advance the SOTA. Expect serious compute for 19×19, even though KataGo showed 50× efficiency vs earlier runs.  ￼

Track B — “Engine‑first SOTA” (product‑grade).
You focus on world‑class search + rule/board/runtimes and initially use existing high‑quality nets (e.g., public KataGo networks) while you stand up training later. This gets a strong engine into GUIs/servers quickly and lets you innovate on search (graph search, Gumbel AZ, batching, inference) with a known‑good policy/value.  ￼

I’ll outline both but optimize for Track B first (fastest tangible results), then layer in training (Track A).

⸻

2) Milestone roadmap

M0 — Board, rules, hashing, I/O (2–3 weeks of core coding work, no ML yet)

Deliverables
	•	Board representation: 19×19 bit‑sets (separate bitboards or packed 361‑bit arrays), fast group/liberty tracking, suicide + ko legality.
	•	Hashing: 64‑bit Zobrist key (side‑to‑move, ko ban, rules parameters) for transpositions and superko.  ￼
	•	Rules: implement a rules engine covering Chinese/Japanese/Korean/AGA, superko variants (positional/situational), area/territory scoring, komi. Provide a rules object that the search respects. Good spec references: KataGo’s rules doc (+ Tromp‑Taylor).  ￼
	•	I/O protocols:
	•	GTP (Go Text Protocol) server for engine integration (Sabaki, Lizzie, OGS adapters).  ￼
	•	SGF load/save (FF[4]) for data and analysis.  ￼

Acceptance tests
	•	Pass Sensei‑style superko edge‑cases; compare your scoring to KataGo rules visualizer for spot checks.  ￼

⸻

M1 — Strong “classical” search kernel (PUCT MCTS) with NN plug‑in points

Deliverables
	•	Search core (PUCT) with:
	•	Q/N statistics per child, prior P, cPUCT schedule, FPU reduction;
	•	Dirichlet noise at root and temperature schedule (opening vs move 50+);
	•	virtual loss for multi‑thread parallelism; tree reuse across moves;
	•	transposition support now or in M3 (graph search).  ￼
	•	Batching hooks: the search issues batched NN eval requests; design an async inference queue to aggregate positions (across threads/games) for GPUs. Research backs batching for Go MCTS.  ￼
	•	Playout cap randomization support (randomly mix shallow/deep searches) — helps value and policy target balance later and also good for play strength diversity.  ￼

Acceptance tests
	•	With a trivial rollout/value (or a tiny policy prior), show the engine plays legal Go and doesn’t hang; profiling shows good NN batch utilization on GPU.

⸻

M2 — NN inference runtime (production‑grade speed)

Deliverables
	•	Backends: ONNX Runtime/TensorRT/CUDA or OpenCL; optionally Eigen CPU fallback. KataGo ships CUDA/OpenCL/Eigen/TensorRT variants—mimic that portability.  ￼
	•	Caching: position‑keyed eval cache (respecting rules config) to cut duplicate NN calls.
	•	Cross‑position batching (analysis engine mode): batch different games/positions into one NN pass (very big speedups).  ￼

Acceptance tests
	•	Sustained NN throughput at target batch sizes; near‑linear speedup with more threads issuing evals.

⸻

M3 — Monte‑Carlo Graph Search (MCGS) with transpositions

Move from a pure tree to a graph (merge nodes by Zobrist + rules). You’ll save compute in joseki/large captures/ko cycles and unlock advanced analysis tooling.

Deliverables
	•	Node table keyed by (Zobrist, rules, ko‑ban), child edges labeled by moves; safe backprop across merged nodes; careful virtual‑loss handling on graphs.
	•	On/off switch & per‑node stats to compare tree vs graph.

References: KataGo’s GraphSearch.md is a great conceptual guide.  ￼

Acceptance tests
	•	Equivalence to tree‑search in small test beds; improved NPS on life‑and‑death with transpositions.

⸻

M4 — Gumbel AlphaZero search option (low‑simulation strength)

Add optional Gumbel AZ policy‑improvement operator (root sampling without replacement + sequential halving; value interpolation for unvisited children). It shines when you want strong play with fewer simulations. Keep PUCT as default; expose a flag.  ￼

Acceptance tests
	•	On fixed budgets (e.g., 100–800 sims), Gumbel mode matches/exceeds baseline PUCT Elo. (Public literature shows strong improvements at few sims.)  ￼

⸻

M5 — Public engine release (no training yet)

Deliverables
	•	GTP binary + config (komi, rules, time settings, visit limits) and a clean JSON analysis output (ownership/score if available from net).
	•	Works in Sabaki/Lizzie/KaTrain; document command‑lines and typical configs.
	•	Optionally ship with a public KataGo net for users to try immediately. (Make license clear—KataGo is MIT.)  ￼

⸻

(Optional but recommended) Track A — Full self‑play training stack

Once the engine runs NN inference, you can build training to reach full SOTA.

A1 — Dataflow & training loop
	•	Self‑play workers: use your engine to generate games, record (s, π, z, score, ownership) and metadata (visits, variance, uncertainty).
	•	Targets & loss (KataGo‑style):
	•	Policy: cross‑entropy vs pruned/forced‑playout root π;
	•	Value: MSE or CE on game outcome;
	•	Ownership + score auxiliary heads (per‑point ownership; final integer score/score distribution);
	•	Auxiliary “next move” policy for regularization.  ￼
	•	Neural arch: pre‑activation ResNet trunk (e.g., 20–40 blocks, 256 channels) with global pooling blocks to inject global context (ko/ladders/endgame).  ￼
	•	Input features: stones/history, side‑to‑move, liberties, move legality masks; KataGo found a few game‑specific features still help.  ￼
	•	Efficiency tricks you should implement from day one:
	•	Playout cap randomization, policy‑target pruning (decouple exploration from the policy target), global pooling, aux heads (ownership/score/next move), dynamic cPUCT; these collectively gave KataGo big speedups.  ￼
	•	Optimizers: AdamW/LAMB with gradient clipping; KataGo uses no per‑layer BN on most nets (see Fixup‑like init / “one BN” variants discussed).  ￼

A2 — Evaluation & gating
	•	Match gating (SPRT‑style): new nets must beat current best by a target win‑rate (e.g., 55% @ 95% confidence) before promotion—this is standard in Leela‑style pipelines.  ￼
	•	Holdout puzzles (endgame, ladders), adversarial suites (cyclic & pass attacks) to avoid regressions.  ￼

A3 — Scale & infra
	•	Start small (single machine) and scale to a distributed mix of self‑play and training workers; the open Minigo project is a good reference for distributed infra.  ￼
	•	Context on compute: earlier runs like AGZ/ELF were huge; KataGo showed strong results on ≪30 GPUs for 19 days thanks to its methods—use those tricks to keep budgets sane.  ￼

⸻

3) Detailed design checklist (things people often get wrong)

Board & rules
	•	Implement both positional and situational superko; many servers differ. Make rules a first‑class parameter of the state and the Zobrist key. Compare behavior to KataGo’s rules page.  ￼

Search
	•	Selection: use PUCT
a^*=\arg\max_a \; Q(s,a) \;+\; U(s,a),\quad U(s,a)=c_{\text{puct}}\cdot P(s,a)\frac{\sqrt{N(s)}}{1+N(s,a)}
with dynamic c_{\text{puct}} and an FPU reduction (treat unseen children as slightly pessimistic). (Form is as in AlphaZero/KataGo.)  ￼
	•	Root exploration: add Dirichlet noise and a temperature > 0 early so self‑play doesn’t collapse to a single line.  ￼
	•	Parallelism: Tree/graph parallelization with virtual loss; root/leaf parallelization are viable too.  ￼
	•	Batching: decouple the “selection” threads from an async evaluation queue so you can hit big GPU batches; this is a large real‑world speed win.  ￼
	•	Graph search: once stable, flip on transposition merging for life‑and‑death and repetitive patterns (see MCGS doc).  ￼
	•	Gumbel AZ (optional): expose a flag for low‑sim regimes; it’s a practical win for fast time controls.  ￼

Network & training
	•	Architecture: pre‑activation ResNet trunk + global pooling pathways feeding heads; heads: policy, value, ownership map, score/score‑dist, aux next‑move. (KataGo ablations show each head/pooling helps.)  ￼
	•	Targets:
	•	Ownership = per‑point probability (black/white/neutral).
	•	Score = integer final score (or distribution).
	•	These targets also stabilize value learning and improve practical play (handicap, endgame).  ￼
	•	Policy target pruning + forced playouts at root to decouple exploration noise from the learning target.  ￼

Robustness (learned from the last 2–3 years)
	•	Include adversarial starting positions (a tiny fraction of self‑play) and a regression test battery; cyclic/pass attacks are known model failures even for superhuman nets.  ￼

⸻

4) Engineering stack
	•	Language: C++ or Rust for the engine core; Python for training.
	•	Backends: CUDA/TensorRT on NVIDIA, OpenCL for portability, CPU fallback; mirror KataGo’s multi‑backend approach.  ￼
	•	Protocols: GTP (engine UI/servers), JSON analysis output; SGF for data.  ￼
	•	Hashing: 64‑bit Zobrist as the canonical key; store rules & ko‑ban in the key to make transpositions correct across different rule params.  ￼
	•	Dev tools: fuzzers for rules/ko; perft‑style enumeration on small boards (5×5/7×7); unit tests for captures/ko/suicide/area vs territory scoring.

⸻

5) Evaluation plan (how you’ll know you’re on track)
	1.	Engine baseline (no training)
	•	With a public strong net, measure Elo vs KataGo (same visits & rules). Aim for parity in speed/strength; if you add graph search or batching, you should improve NPS and/or fast‑TC strength.  ￼
	2.	Self‑play learning
	•	Track Elo vs a frozen baseline at fixed visits; require gating wins (SPRT‑style) before promoting a net.  ￼
	•	Ablate: turn off global pooling / ownership+score / policy‑target pruning—your curves should mirror the KataGo ablation patterns (big efficiency drops).  ￼
	•	Curriculum puzzles (ladders, seki, snapback), endgame exact puzzles: you should see steady accuracy improvements. (Recent work evaluates KataGo on perfect endgames.)  ￼
	•	Robustness: include the FAR AI adversarial suite in CI; track worst‑case results.  ￼

⸻

6) Minimal algorithms you can implement right now

PUCT selection (per child)
	•	Maintain N, W, Q=W/N, P.
	•	Select child a maximizing Q + c_{\text{puct}}\,P\sqrt{N_\text{parent}}/(1+N).
	•	Apply virtual loss when a thread commits to a child; undo on backprop.  ￼

Root exploration
	•	Add Dirichlet noise to P at the root; sample‑temperature \tau>0 for early moves.  ￼

Policy‑target pruning (for training)
	•	After search, force some minimal playouts on promising moves (for exploration), then prune those forced playouts from the actual training target so the policy isn’t trained toward noise; log‑softmax the pruned distribution.  ￼

Ownership/score heads
	•	Ownership: per point BCE/CE vs final territory/stone ownership; Score: MSE to final integer score or KL to a score distribution; weight these heads moderately (they regularize value).  ￼

Graph search
	•	Key nodes by (Zobrist, rules, ko‑ban). When a transposition is encountered, link to existing node; backprop aggregates through the DAG. Read KataGo’s GraphSearch.md for pitfalls (credit assignment, virtual loss on graphs).  ￼

Gumbel AZ mode
	•	At root, sample actions without replacement using Gumbel‑Top‑k, allocate sims via sequential halving, and pick the survivor; at non‑roots, use value interpolation for unvisited actions.  ￼

⸻

7) Data & infrastructure notes
	•	Starting data: supervised warm‑start from SGF databases is optional (pure self‑play works), but an opening “policy‑only” warm‑start can stabilize very small runs. (Minigo has examples of pipelines.)  ￼
	•	Distributed: a simple queue of self‑play workers → training → evaluation (gating). Leverage TF/PyTorch distributed or Kubernetes‑style orchestration; Minigo’s infra is a reference.  ￼
	•	Compute expectations: historical ballparks—AlphaZero/ELF used thousands of TPUs/GPUs; KataGo reduced this drastically via the methods above (surpassed ELF’s final model with <30 GPUs in 19 days). Plan your ambitions accordingly.  ￼

⸻

8) Security/robustness & fairness

Build adversarial tests (cyclic, pass‑attack) into your evaluation cycles and sprinkle adversarial starts (~0.1%) into self‑play; this has already been tried in KataGo training. Keep a clear “worst‑case” dashboard, not just average Elo.  ￼

⸻

9) Interface & packaging
	•	GTP as the canonical engine interface so users can drop your engine into Sabaki/Lizzie; document rule configuration and time controls clearly.  ￼
	•	Analysis mode: JSON streaming with principal variations, ownership map, score estimate, and uncertainty (variance). The latter is useful for playout allocation (uncertainty‑weighted playouts). (Methods documented in KataGo’s notes.)  ￼

⸻

10) Reading list (the essentials to have open while you build)
	•	KataGo methods & paper (must‑read):
– Accelerating Self‑Play Learning in Go (arXiv + HTML mirror) – methods, losses, ablations.  ￼
– KataGo docs on Graph Search and analysis/inference.  ￼
	•	AlphaGo Zero (for the core baseline): network, PUCT, root noise/temperature.  ￼
	•	Batching & parallel MCTS (practical speed): batch‑MCTS and virtual loss/parallelization.  ￼
	•	Gumbel AlphaZero (effective at low sims).  ￼
	•	Rules & protocols: GTP v2, SGF FF[4], KataGo rules.  ￼
	•	Robustness: adversarial policies vs Go AIs (FAR AI).  ￼

⸻

11) Suggested first 4 sprints (action checklist)
	1.	Engine skeleton: board/rules/Zobrist + GTP + SGF I/O; perft‑like tests; match KataGo rules behavior on edge cases.  ￼
	2.	Search + inference: PUCT with virtual loss; async batched NN inference; ship with a public net to validate strength quickly.  ￼
	3.	Graph search & analysis output (ownership/score if your net provides them).  ￼
	4.	Gumbel mode + benchmarking at low simulation budgets; package a clean release with configs for Chinese/Japanese rules & common komi.  ￼

⸻

Final note

If your goal is new SOTA, push on (a) graph search quality and (b) training targets/loss shaping (ownership/score/next‑move/uncertainty), while maintaining robustness to adversarial positions. Those are the highest‑leverage areas suggested by the last few years of research and practice.  ￼

If you want, I can sketch a starter code layout for the engine core (board, rules, transpositions, PUCT, inference queue) in C++ or Rust, and a minimal PyTorch model/loader that matches these heads and outputs.


Great—Mac + non‑neural it is. Below is a road map for a top‑tier, non‑NN Go engine in 2025. It leans on the strongest classical stack ever built (MoGo → Fuego → Pachi): UCT‑style MCTS + RAVE/AMAF, heavy playout policies, dynamic komi, progressive bias/widening, and tactical solvers (DFPN/MCTS‑Solver) for life‑&‑death. This is the ceiling for non‑NN Go; it won’t reach modern NN engines, but it will get you a strong, elegant system that’s fast and portable on macOS.  ￼

⸻

Milestones (what to build, in order)

M0 — Board, rules, hashing, I/O (foundation)
	•	Rules/Scoring: implement Tromp–Taylor (area scoring, positional superko) first; add toggles for AGA/Japanese/Korean as thin wrappers. Tromp–Taylor is compact and unambiguous—ideal for engines and rollouts.  ￼
	•	Ko / superko: support positional and situational superko (parameterized) to match servers & GUIs.  ￼
	•	Zobrist hashing (64‑bit) keyed by (board, side‑to‑move, superko variant). Use it both for repetition checks and (later) MCTS transposition table.  ￼
	•	GTP: speak the Go Text Protocol v2 (think: protocol_version, boardsize, clear_board, play, genmove, final_score). This instantly plugs into Sabaki/KaTrain/etc.  ￼

Acceptance tests: a corpus of superko edge cases; scoring parity with Tromp–Taylor reference on random boards; GTP echo tests.  ￼

⸻

M1 — Core search: UCT‑MCTS with RAVE (MC‑RAVE)

Build a clean UCT loop (select → expand → playout → backprop), then add RAVE/AMAF blending (MC‑RAVE). MC‑RAVE shares statistics of “moves played later” to speed early estimates; it was the key boost in pre‑NN Go.  ￼

Details to include
	•	UCT selection with UCB1 on nodes; decaying RAVE weight from high (few visits) to low (many visits).  ￼
	•	Progressive bias: add a heuristic prior term (shape/pattern scores) that fades out with visits; progressive widening/unpruning controls branching.  ￼

Acceptance tests: on 9×9, confirm RAVE accelerates win‑rate vs pure UCT at fixed playouts (standard result).  ￼

⸻

M2 — Playout policy: from light → heavy

Random rollouts are too noisy. Implement a heavy playout policy patterned after Fuego/MoGo/Pachi:
	1.	Urgent rules first: immediate captures, atari defense, liberties‑2 emergencies.
	2.	3×3 / local patterns around last moves (and simple distance‑2/3 shapes).
	3.	Self‑atari / eye‑filling avoidance (unless tactical).
	4.	Nakade checks after captures; optional “fillboard” safety rules.  ￼

These heuristics are well‑documented in Fuego and MoGo write‑ups; they were the backbone of strong pre‑NN engines.  ￼

Acceptance tests: playout length distribution stabilizes; “atari blunders” largely gone; strength increase vs light rollouts on 9×9.  ￼

⸻

M3 — “History” enhancements for playouts

Make simulations smarter without NN:
	•	MAST (Move‑Average Sampling Technique): global (move→win‑rate) table guiding playout choices via softmax.  ￼
	•	LGRF (Last‑Good‑Reply with Forgetting): remember “good replies” to opponents’ last moves; decays over time.  ￼
	•	(Optional) N‑gram/sequence statistics for short local replies (often combined with MAST).  ￼

Acceptance tests: at fixed playouts, show Elo gain over M2 with (MAST+LGRF) on 9×9/13×13 suites.  ￼

⸻

M4 — Endgame & advantage handling
	•	Dynamic Komi: adjust komi on‑the‑fly during search to keep win‑rate in a useful band (improves handicap/endgame stability). Ships in Pachi; widely used.  ￼
	•	Score‑bounded MCTS (optional): prune/plow lines by score windows to improve endgame decisions.  ￼

Acceptance tests: large handicap games stop “resigning winning positions”; endgame score error shrinks with dynamic komi.  ￼

⸻

M5 — Tactical solver integration (big strength jump in L&D)

Blend MCTS with exact solvers for life‑&‑death:
	•	DFPN (Depth‑First Proof‑Number Search) as a local solver for bounded regions (ladders, small eyespace, semeai). Proven effective in Go tsumego.  ￼
	•	MCTS‑Solver backprop: allow “proven wins/losses” to override statistics and prove subtrees—classic technique from LOA, also used in Go components.  ￼

Acceptance tests: a L&D test set where the engine returns proven results; lower blunder rate in capturing races.  ￼

⸻

M6 — Parallelization, time, and transpositions
	•	Parallel MCTS: support root, leaf, and tree parallelization; virtual loss + fine‑grained locks for tree parallelism; scalable on multi‑core Apple Silicon.  ￼
	•	Time management (by uncertainty / criticality): classical study shows sizable Elo gains in Go—implement a policy akin to Coulom et al. (pondering optional).  ￼
	•	(Optional) Transpositions: Go has few, but a Zobrist‑keyed TT reduces duplication in ko/semeai reading.  ￼

Acceptance tests: near‑linear speedup to ~8–10 threads with tree parallelization on M‑series; time‑use profiles match Coulom‑style schedules.  ￼

⸻

M7 — Packaging & UX
	•	GTP engine binary + config file (rules=tt, superko=positional/situational, dynkomi=on, threads=N, max_playouts etc.).  ￼
	•	Works out of the box in Sabaki (macOS) and other GUIs.  ￼

⸻

Mac‑first build & performance notes
	•	Language: C++20 (or Rust if you prefer) for engine; single portable binary for Apple Silicon & Intel Macs.
	•	Parallelism: start with std::thread (portable). If you want OpenMP on macOS’s Apple Clang, install libomp via Homebrew and either let CMake FindOpenMP handle flags, or add -Xpreprocessor -fopenmp -lomp manually.  ￼
	•	CMake sketch:

find_package(OpenMP)
if(OpenMP_CXX_FOUND)
  target_link_libraries(your_engine PRIVATE OpenMP::OpenMP_CXX)
endif()

(On macOS, this works once brew install libomp is present.)  ￼

	•	Binary flags: -O3 -flto -fstrict-aliasing (avoid -Ofast until you’ve verified FP determinism). Use -mcpu=native on your Apple Silicon for NEON; profile with Instruments.

⸻

Data structures & algorithms you’ll want

Board & groups
	•	361‑cell board; per‑color bitmaps (e.g., 6×uint64_t) + a group table (union‑find or linked blocks) tracking liberties efficiently.
	•	Benson’s algorithm for unconditional life (cheap static checks in eval/termination).  ￼

Search objects
	•	Node: {N, W, Q, UCB terms, RAVE stats (N_rave, Q_rave), priors (heuristic), children[]}.
	•	RAVE mix: Q_blend = (β(n) * Q_rave + (1-β(n)) * Q_uct) with β decaying as visits grow.  ￼

Playout policy
	•	Priority ordering: atari-capture → atari-defense → liberty‑2 moves → 3×3 patterns near last two moves → nakade after captures → random legal (no self‑atari/eye). (Fuego/MoGo lineage.)  ￼

Extras that matter
	•	Progressive bias/widening constants tuned per board size.  ￼
	•	MAST / LGRF tables with exponential decay between moves.  ￼
	•	Dynamic komi schedule tied to root win‑rate & search depth.  ￼
	•	DFPN hook: if a leaf is a compact L&D region, switch to DFPN; if solved, backprop a proven result (MCTS‑Solver style).  ￼

⸻

Test & benchmarking plan (non‑NN)
	1.	9×9 strength (fast iterate): fixed playout budgets; compare variants: UCT → +RAVE → +heavy playouts → +MAST/LGRF → +dyn komi → +DFPN hook. The monotone gains you see should mirror classical literature.  ￼
	2.	13×13/19×19 scaling: check threads vs NPS; lock contention; virtual‑loss tuning.  ￼
	3.	Endgame/handicap suites for dynamic komi & score‑bounded search.  ￼

⸻

Minimal code skeleton (C++ outline)

The following shows shape (trim for brevity). It compiles cleanly once you add your board & move types.

struct Node {
  std::atomic<int> N{0};
  std::atomic<double> W{0.0};      // sum of results from the current player's POV
  double Q() const { int n=N.load(); return n ? W.load()/n : 0.0; }

  // RAVE stats
  std::atomic<int> N_rave{0};
  std::atomic<double> W_rave{0.0};

  // Heuristic prior for progressive bias (e.g., pattern score)
  double H_prior = 0.0;

  std::vector<Move> moves;
  std::vector<Node*> children;
  std::mutex expand_mtx;
};

double uct_score(const Node& p, const Node& c, double c_puct, double beta_rave) {
  const int n_p = p.N.load();
  const int n_c = std::max(1, c.N.load());
  const double q_uct  = c.Q();
  const double q_rave = c.N_rave.load() ? c.W_rave.load()/c.N_rave.load() : 0.0;
  const double q_blend = beta_rave*q_rave + (1.0-beta_rave)*q_uct;

  // Progressive bias term fades with visits
  const double prog_bias = c.H_prior / (1.0 + n_c);

  return q_blend + prog_bias + c_puct * std::sqrt(std::max(1, n_p)) / (1 + n_c);
}

Add virtual‑loss on selection, do heavy playouts, backprop both UCT and RAVE stats, and handle DFPN hooks at leaves.

⸻

macOS developer ergonomics
	•	GUI for testing: point Sabaki at your engine (gtp path) via Preferences → Engines; it speaks GTP and just works.  ￼
	•	OpenMP on Apple Clang (optional): brew install libomp, then either CMake OpenMP::OpenMP_CXX or manual flags -Xpreprocessor -fopenmp -lomp.  ￼

⸻

What “state‑of‑the‑art (non‑NN)” includes
	•	UCT + RAVE as the search backbone (MoGo → Fuego → Pachi).  ￼
	•	Heavy playout policies with tactical heuristics & 3×3 patterns.  ￼
	•	Progressive bias/widening to inject domain knowledge while keeping UCT sound.  ￼
	•	MAST / LGRF to reuse statistical experience within a game.  ￼
	•	Dynamic komi for advantage/handicap stability.  ￼
	•	DFPN / MCTS‑Solver for life‑&‑death proofs inside the search.  ￼

These are exactly the levers that made Fuego and Pachi the strongest non‑NN open engines.  ￼

⸻

Compact reading list (primary sources)
	•	UCT: Kocsis & Szepesvári (2006).  ￼
	•	RAVE / MC‑RAVE: Gelly & Silver (2011).  ￼
	•	Fuego (engine & heuristics): Enzenberger et al., TCIAIG 2010 + API docs.  ￼
	•	Pachi (RAVE engine): Baudiš & Gailly, LNCS 2012.  ￼
	•	Dynamic Komi: Baudiš (ICGA 2011).  ￼
	•	MAST & N‑grams: Björnsson & Finnsson lineage; CIG’13 analysis.  ￼
	•	LGRF: Baier & Drake, IEEE TCIAIG 2010.  ￼
	•	MCTS parallelization & virtual loss: Chaslot/Winands et al. (2008).  ￼
	•	Time management in Go MCTS: Coulom et al. (2010).  ￼
	•	DFPN in Go tsumego: Kishimoto & Müller (AAAI’05). MCTS‑Solver: Winands et al.  ￼
	•	Tromp–Taylor & GTP specs (engine I/O).  ￼

⸻

Want a starter repo layout?

If helpful, I can give you a ready‑to‑build skeleton (CMake + minimal GTP server + UCT/RAVE core + heavy playout hooks + DFPN stub) tailored for Apple Silicon, along with a few regression tests (ko/superko, nakade, ladders) and a Sabaki config snippet.

Awesome—here’s a ready‑to‑build starter repo for a non‑neural, macOS‑friendly Go engine. It compiles to a GTP binary you can point Sabaki/KaTrain at, and it’s structured so you can iterate on UCT+RAVE, heavy playouts, and (later) add parallelism, MAST/LGRF, dynamic komi, or DFPN hooks.

Download: classic-go-engine.zip

⸻

What’s inside

Highlights
	•	Tromp–Taylor rules (area scoring) with positional superko.
	•	Clean Board (captures, suicide checks, ko, pass), GTP I/O.
	•	MCTS (UCT) + RAVE blending (simple, readable).
	•	Heavy playout policy (capture/defend‑atari/avoid self‑atari → random legal).
	•	Minimal but correct final_score (TT area + komi).
	•	CMake project that builds cleanly on Apple Silicon; optional OpenMP flag is already wired.

Layout

classic-go-engine/
  CMakeLists.txt
  LICENSE  (MIT)
  README.md
  include/
    Board.hpp        # board state, Zobrist, TT scoring, PSK
    GTP.hpp          # tiny GTP server
    MCTS.hpp         # UCT + RAVE
    PlayoutPolicy.hpp
    util.hpp
  src/
    main.cpp         # GTP entry; knobs (playouts, c_uct, rave_equiv)
    go/Board.cpp
    gtp/GTP.cpp
    mcts/MCTS.cpp
    mcts/PlayoutPolicy.cpp
  tests/
    board_ko_test.cpp  # stub smoke test; extend with proper ko/suicide cases


⸻

Build & run on macOS (Apple Silicon or Intel)

unzip classic-go-engine.zip
cd classic-go-engine
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
cmake --build . -j
./classicgo

Optional OpenMP (if you want to parallelize later):

brew install libomp
cmake -DUSE_OPENMP=ON -DCMAKE_BUILD_TYPE=Release ..
cmake --build . -j

Try it via GTP

protocol_version
name
boardsize 9
clear_board
komi 7.5
genmove B
play W D4
genmove B
final_score
quit

Or add the binary to Sabaki (Preferences → Engines) and just play.

⸻

How the engine works (quick tour)

Board (Tromp–Taylor + PSK)
	•	0‑based (x,y) grid, captures via flood‑fill groups/liberties.
	•	Zobrist positional hash tracks every position; a set of past hashes enforces positional superko.
	•	Suicide is treated as illegal (TT default).
	•	tromp_taylor_score() does stone+territory flood‑fills and subtracts komi.

MCTS (UCT + RAVE)
	•	Nodes hold per‑player values; child selection uses UCB1 on a RAVE‑blended mean:
Q_\text{blend} = (1-\beta) Q_\text{UCT} + \beta Q_\text{RAVE},\quad
\beta = \frac{\text{rave\_equiv}}{N_c+\text{rave\_equiv}}
with \text{rave\_equiv} configurable (default 1000).
	•	Expansion: one unexpanded child at a time.
	•	Rollouts: two‑pass end or max playout moves → TT score → +1 (B win) / −1 (W win).
	•	RAVE updates: AMAF sets per color from the rollout update parent nodes’ move→(n,w) stats.

Playout policy (simple “heavy” rules)
	1.	Immediate captures if available.
	2.	Atari defense (coarse heuristic) if available.
	3.	Avoid self‑atari, else random legal.
It’s intentionally lean so you can slot in more rules (3×3/shape patterns, nakade, LGRF/MAST).

⸻

What to add next (incremental upgrades)
	1.	MAST + LGRF (biggest quick win):
	•	Keep a small table of (move→winrate) and last‑good‑reply by opponent move.
	•	Use a softmax bias in rollouts; decay the tables between moves.
	2.	Progressive bias / widening:
	•	Inject a simple pattern prior H_prior into UCT that fades with visits; limit branching early.
	3.	Dynamic Komi:
	•	Adjust komi at the root based on current win‑rate band for better handicap & endgame behavior.
	4.	DFPN hook for L&D:
	•	When the leaf is a compact fight (few liberties / small region), call a tiny DFPN solver and backprop “proven” results (MCTS‑Solver style).
	5.	Parallel MCTS:
	•	Add tree parallelization with virtual loss and a simple lock per node; then consider OpenMP.
	6.	Robust regression tests:
	•	Ko/superko edge cases, seki, suicide, nakade; a small 9×9 perft‑style enumerator helps catch rule bugs.

⸻

Tune knobs
	•	MCTSConfig in src/main.cpp:
	•	playouts (e.g., 800–10k for 9×9; start small for 19×19)
	•	c_uct (try 1.2–2.0)
	•	rave_equiv (e.g., 300–1500; higher → more RAVE early)
	•	max_playout_moves (safety cap)

⸻

If you want, I can ship a follow‑up patch that adds MAST + LGRF to this codebase (cleanly wired into PlayoutPolicy and the rollout loop) and a small set of 3×3 patterns for playouts. ￼
